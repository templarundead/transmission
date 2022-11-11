// This file Copyright © 2008-2022 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <array>
#include <functional>
#include <memory>
#include <utility>

#include <giomm.h> /* g_file_trash() */
#include <glibmm/i18n.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(4, 0, 0) && defined(GDK_WINDOWING_X11)
#include <gdk/x11/gdkx.h>
#endif

#include <fmt/core.h>

#include <libtransmission/transmission.h> /* TR_RATIO_NA, TR_RATIO_INF */

#include <libtransmission/error.h>
#include <libtransmission/torrent-metainfo.h>
#include <libtransmission/utils.h> /* tr_strratio() */
#include <libtransmission/version.h> /* SHORT_VERSION_STRING */
#include <libtransmission/web-utils.h>

#include "Prefs.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"

using namespace std::literals;

/***
****  UNITS
***/

int const mem_K = 1024;
char const* const mem_K_str = N_("KiB");
char const* const mem_M_str = N_("MiB");
char const* const mem_G_str = N_("GiB");
char const* const mem_T_str = N_("TiB");

int const disk_K = 1000;
char const* const disk_K_str = N_("kB");
char const* const disk_M_str = N_("MB");
char const* const disk_G_str = N_("GB");
char const* const disk_T_str = N_("TB");

int const speed_K = 1000;
char const* const speed_K_str = N_("kB/s");
char const* const speed_M_str = N_("MB/s");
char const* const speed_G_str = N_("GB/s");
char const* const speed_T_str = N_("TB/s");

/***
****
***/

Glib::ustring gtr_get_unicode_string(GtrUnicode uni)
{
    switch (uni)
    {
    case GtrUnicode::Up:
        return "\xE2\x96\xB4";

    case GtrUnicode::Down:
        return "\xE2\x96\xBE";

    case GtrUnicode::Inf:
        return "\xE2\x88\x9E";

    case GtrUnicode::Bullet:
        return "\xE2\x88\x99";

    default:
        return "err";
    }
}

Glib::ustring tr_strlratio(double ratio)
{
    return tr_strratio(ratio, gtr_get_unicode_string(GtrUnicode::Inf).c_str());
}

Glib::ustring tr_strlsize(guint64 size_in_bytes)
{
    return size_in_bytes == 0 ? Q_("None") : tr_formatter_size_B(size_in_bytes);
}

namespace
{

std::string tr_format_future_time(time_t seconds)
{
    if (auto const days_from_now = seconds / 86400U; days_from_now > 0U)
    {
        return fmt::format(
            ngettext("{days_from_now:L} day from now", "{days_from_now:L} days from now", days_from_now),
            fmt::arg("days_from_now", days_from_now));
    }

    if (auto const hours_from_now = (seconds % 86400U) / 3600U; hours_from_now > 0U)
    {
        return fmt::format(
            ngettext("{hours_from_now:L} hour from now", "{hours_from_now:L} hours from now", hours_from_now),
            fmt::arg("hours_from_now", hours_from_now));
    }

    if (auto const minutes_from_now = (seconds % 3600U) / 60U; minutes_from_now > 0U)
    {
        return fmt::format(
            ngettext("{minutes_from_now:L} minute from now", "{minutes_from_now:L} minutes from now", minutes_from_now),
            fmt::arg("minutes_from_now", minutes_from_now));
    }

    if (auto const seconds_from_now = seconds % 60U; seconds_from_now > 0U)
    {
        return fmt::format(
            ngettext("{seconds_from_now:L} second from now", "{seconds_from_now:L} seconds from now", seconds_from_now),
            fmt::arg("seconds_from_now", seconds_from_now));
    }

    return _("now");
}

std::string tr_format_past_time(time_t seconds)
{
    if (auto const days_ago = seconds / 86400U; days_ago > 0U)
    {
        return fmt::format(ngettext("{days_ago:L} day ago", "{days_ago:L} days ago", days_ago), fmt::arg("days_ago", days_ago));
    }

    if (auto const hours_ago = (seconds % 86400U) / 3600U; hours_ago > 0U)
    {
        return fmt::format(
            ngettext("{hours_ago:L} hour ago", "{hours_ago:L} hours ago", hours_ago),
            fmt::arg("hours_ago", hours_ago));
    }

    if (auto const minutes_ago = (seconds % 3600U) / 60U; minutes_ago > 0U)
    {
        return fmt::format(
            ngettext("{minutes_ago:L} minute ago", "{minutes_ago:L} minutes ago", minutes_ago),
            fmt::arg("minutes_ago", minutes_ago));
    }

    if (auto const seconds_ago = seconds % 60U; seconds_ago > 0U)
    {
        return fmt::format(
            ngettext("{seconds_ago:L} second ago", "{seconds_ago:L} seconds ago", seconds_ago),
            fmt::arg("seconds_ago", seconds_ago));
    }

    return _("now");
}

} // namespace

std::string tr_format_time(time_t timestamp)
{
    if (auto const days = timestamp / 86400U; days > 0U)
    {
        return fmt::format(ngettext("{days:L} day", "{days:L} days", days), fmt::arg("days", days));
    }

    if (auto const hours = (timestamp % 86400U) / 3600U; hours > 0U)
    {
        return fmt::format(ngettext("{hours:L} hour", "{hours:L} hours", hours), fmt::arg("hours", hours));
    }

    if (auto const minutes = (timestamp % 3600U) / 60U; minutes > 0U)
    {
        return fmt::format(ngettext("{minutes:L} minute", "{minutes:L} minutes", minutes), fmt::arg("minutes", minutes));
    }

    if (auto const seconds = timestamp % 60U; seconds > 0U)
    {
        return fmt::format(ngettext("{seconds:L} second", "{seconds:L} seconds", seconds), fmt::arg("seconds", seconds));
    }

    return _("now");
}

std::string tr_format_time_left(time_t timestamp)
{
    if (auto const days_left = timestamp / 86400U; days_left > 0U)
    {
        return fmt::format(
            ngettext("{days_left:L} day left", "{days_left:L} days left", days_left),
            fmt::arg("days_left", days_left));
    }

    if (auto const hours_left = (timestamp % 86400U) / 3600U; hours_left > 0U)
    {
        return fmt::format(
            ngettext("{hours_left:L} hour left", "{hours_left:L} hours left", hours_left),
            fmt::arg("hours_left", hours_left));
    }

    if (auto const minutes_left = (timestamp % 3600U) / 60U; minutes_left > 0U)
    {
        return fmt::format(
            ngettext("{minutes_left:L} minute left", "{minutes_left:L} minutes left", minutes_left),
            fmt::arg("minutes_left", minutes_left));
    }

    if (auto const seconds_left = timestamp % 60U; seconds_left > 0U)
    {
        return fmt::format(
            ngettext("{seconds_left:L} second left", "{seconds_left:L} seconds left", seconds_left),
            fmt::arg("seconds_left", seconds_left));
    }

    return _("now");
}

std::string tr_format_time_relative(time_t timestamp, time_t origin)
{
    return timestamp < origin ? tr_format_future_time(origin - timestamp) : tr_format_past_time(timestamp - origin);
}

namespace
{

Gtk::Window* getWindow(Gtk::Widget* w)
{
    if (w == nullptr)
    {
        return nullptr;
    }

    if (auto* const window = dynamic_cast<Gtk::Window*>(w); window != nullptr)
    {
        return window;
    }

    return static_cast<Gtk::Window*>(w->get_ancestor(Gtk::Window::get_type()));
}

} // namespace

void gtr_add_torrent_error_dialog(Gtk::Widget& child, tr_torrent* duplicate_torrent, std::string const& filename)
{
    Glib::ustring secondary;
    auto* win = getWindow(&child);

    if (duplicate_torrent != nullptr)
    {
        secondary = fmt::format(
            _("The torrent file '{path}' is already in use by '{torrent_name}'."),
            fmt::arg("path", filename),
            fmt::arg("torrent_name", tr_torrentName(duplicate_torrent)));
    }
    else
    {
        secondary = fmt::format(_("Couldn't add torrent file '{path}'"), fmt::arg("path", filename));
    }

    auto w = std::make_shared<Gtk::MessageDialog>(
        *win,
        _("Couldn't open torrent"),
        false,
        TR_GTK_MESSAGE_TYPE(ERROR),
        TR_GTK_BUTTONS_TYPE(CLOSE));
    w->set_secondary_text(secondary);
    w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
    w->show();
}

/* pop up the context menu if a user right-clicks.
   if the row they right-click on isn't selected, select it. */
bool on_tree_view_button_pressed(
    Gtk::TreeView& view,
    double view_x,
    double view_y,
    bool context_menu_requested,
    std::function<void(double, double)> const& callback)
{
    if (context_menu_requested)
    {
        Gtk::TreeModel::Path path;

        if (auto const selection = view.get_selection();
            view.get_path_at_pos((int)view_x, (int)view_y, path) && !selection->is_selected(path))
        {
            selection->unselect_all();
            selection->select(path);
        }

        if (callback)
        {
            callback(view_x, view_y);
        }

        return true;
    }

    return false;
}

/* if the user clicked in an empty area of the list,
 * clear all the selections. */
bool on_tree_view_button_released(Gtk::TreeView& view, double view_x, double view_y)
{
    if (Gtk::TreeModel::Path path; !view.get_path_at_pos((int)view_x, (int)view_y, path))
    {
        view.get_selection()->unselect_all();
    }

    return false;
}

void setup_tree_view_button_event_handling(
    Gtk::TreeView& view,
    std::function<bool(guint, TrGdkModifierType, double, double, bool)> const& press_callback,
    std::function<bool(double, double)> const& release_callback)
{
#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto controller = Gtk::GestureClick::create();
    controller->set_button(0);
    controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    if (press_callback)
    {
        controller->signal_pressed().connect(
            [&view, press_callback, controller](int /*n_press*/, double event_x, double event_y)
            {
                auto* const sequence = controller->get_current_sequence();
                auto const event = controller->get_last_event(sequence);
                if (event->get_event_type() == TR_GDK_EVENT_TYPE(BUTTON_PRESS) &&
                    press_callback(
                        event->get_button(),
                        event->get_modifier_state(),
                        event_x,
                        event_y,
                        event->triggers_context_menu()))
                {
                    controller->set_sequence_state(sequence, Gtk::EventSequenceState::CLAIMED);
                }
            },
            false);
    }
    if (release_callback)
    {
        controller->signal_released().connect(
            [&view, release_callback, controller](int /*n_press*/, double event_x, double event_y)
            {
                auto* const sequence = controller->get_current_sequence();
                auto const event = controller->get_last_event(sequence);
                if (event->get_event_type() == TR_GDK_EVENT_TYPE(BUTTON_RELEASE) && release_callback(event_x, event_y))
                {
                    controller->set_sequence_state(sequence, Gtk::EventSequenceState::CLAIMED);
                }
            });
    }
    view.add_controller(controller);
#else
    if (press_callback)
    {
        view.signal_button_press_event().connect(
            [press_callback](GdkEventButton* event)
            { return press_callback(event->button, event->state, event->x, event->y, event->button == GDK_BUTTON_SECONDARY); },
            false);
    }
    if (release_callback)
    {
        view.signal_button_release_event().connect([release_callback](GdkEventButton* event)
                                                   { return release_callback(event->x, event->y); });
    }
#endif
}

bool gtr_file_trash_or_remove(std::string const& filename, tr_error** error)
{
    bool trashed = false;
    bool result = true;

    g_return_val_if_fail(!filename.empty(), false);

    auto const file = Gio::File::create_for_path(filename);

    if (gtr_pref_flag_get(TR_KEY_trash_can_enabled))
    {
        try
        {
            trashed = file->trash();
        }
        catch (Glib::Error const& e)
        {
            g_message(
                "%s",
                fmt::format(
                    _("Couldn't move '{path}' to trash: {error} ({error_code})"),
                    fmt::arg("path", filename),
                    fmt::arg("error", TR_GLIB_EXCEPTION_WHAT(e)),
                    fmt::arg("error_code", e.code()))
                    .c_str());
            tr_error_set(error, e.code(), TR_GLIB_EXCEPTION_WHAT(e));
        }
    }

    if (!trashed)
    {
        try
        {
            file->remove();
        }
        catch (Glib::Error const& e)
        {
            g_message(
                "%s",
                fmt::format(
                    _("Couldn't remove '{path}': {error} ({error_code})"),
                    fmt::arg("path", filename),
                    fmt::arg("error", TR_GLIB_EXCEPTION_WHAT(e)),
                    fmt::arg("error_code", e.code()))
                    .c_str());
            tr_error_clear(error);
            tr_error_set(error, e.code(), TR_GLIB_EXCEPTION_WHAT(e));
            result = false;
        }
    }

    return result;
}

Glib::ustring gtr_get_help_uri()
{
    static auto const uri = fmt::format("https://transmissionbt.com/help/gtk/{}.{}x", MAJOR_VERSION, MINOR_VERSION / 10);
    return uri;
}

void gtr_open_file(std::string const& path)
{
    gtr_open_uri(Gio::File::create_for_path(path)->get_uri());
}

void gtr_open_uri(Glib::ustring const& uri)
{
    if (!uri.empty())
    {
        bool opened = false;

        if (!opened)
        {
            try
            {
                opened = Gio::AppInfo::launch_default_for_uri(uri);
            }
            catch (Glib::Error const&)
            {
            }
        }

        if (!opened)
        {
            try
            {
                Glib::spawn_async({}, std::vector<std::string>{ "xdg-open", uri }, TR_GLIB_SPAWN_FLAGS(SEARCH_PATH));
                opened = true;
            }
            catch (Glib::SpawnError const&)
            {
            }
        }

        if (!opened)
        {
            g_message("%s", fmt::format(_("Couldn't open '{url}'"), fmt::arg("url", uri)).c_str());
        }
    }
}

/***
****
***/

namespace
{

class EnumComboModelColumns : public Gtk::TreeModelColumnRecord
{
public:
    EnumComboModelColumns()
    {
        add(value);
        add(label);
    }

    Gtk::TreeModelColumn<int> value;
    Gtk::TreeModelColumn<Glib::ustring> label;
};

EnumComboModelColumns const enum_combo_cols;

} // namespace

void gtr_combo_box_set_active_enum(Gtk::ComboBox& combo_box, int value)
{
    auto const& column = enum_combo_cols.value;

    /* do the value and current value match? */
    if (auto const iter = combo_box.get_active(); iter)
    {
        if (iter->get_value(column) == value)
        {
            return;
        }
    }

    /* find the one to select */
    for (auto const& row : combo_box.get_model()->children())
    {
        if (row.get_value(column) == value)
        {
            combo_box.set_active(TR_GTK_TREE_MODEL_CHILD_ITER(row));
            return;
        }
    }
}

Gtk::ComboBox* gtr_combo_box_new_enum(std::vector<std::pair<Glib::ustring, int>> const& items)
{
    auto* w = Gtk::make_managed<Gtk::ComboBox>();
    gtr_combo_box_set_enum(*w, items);
    return w;
}

void gtr_combo_box_set_enum(Gtk::ComboBox& combo, std::vector<std::pair<Glib::ustring, int>> const& items)
{
    auto store = Gtk::ListStore::create(enum_combo_cols);

    for (auto const& [label, value] : items)
    {
        auto const iter = store->append();
        (*iter)[enum_combo_cols.value] = value;
        (*iter)[enum_combo_cols.label] = label;
    }

    combo.clear();
    combo.set_model(store);

    auto* r = Gtk::make_managed<Gtk::CellRendererText>();
    combo.pack_start(*r, true);
    combo.add_attribute(r->property_text(), enum_combo_cols.label);
}

int gtr_combo_box_get_active_enum(Gtk::ComboBox const& combo_box)
{
    int value = 0;

    if (auto const iter = combo_box.get_active(); iter)
    {
        iter->get_value(0, value);
    }

    return value;
}

Gtk::ComboBox* gtr_priority_combo_new()
{
    auto* w = Gtk::make_managed<Gtk::ComboBox>();
    gtr_priority_combo_init(*w);
    return w;
}

void gtr_priority_combo_init(Gtk::ComboBox& combo)
{
    gtr_combo_box_set_enum(
        combo,
        {
            { _("High"), TR_PRI_HIGH },
            { _("Normal"), TR_PRI_NORMAL },
            { _("Low"), TR_PRI_LOW },
        });
}

/***
****
***/

namespace
{

auto const ChildHiddenKey = Glib::Quark("gtr-child-hidden");

} // namespace

void gtr_widget_set_visible(Gtk::Widget& w, bool b)
{
    /* toggle the transient children, too */
    if (auto const* const window = dynamic_cast<Gtk::Window*>(&w); window != nullptr)
    {
        auto top_levels = Gtk::Window::list_toplevels();

        for (auto top_level_it = top_levels.begin(); top_level_it != top_levels.end();)
        {
            auto* const l = *top_level_it++;

            if (l->get_transient_for() != window)
            {
                continue;
            }

            if (l->get_visible() == b)
            {
                continue;
            }

            if (b && l->get_data(ChildHiddenKey) != nullptr)
            {
                l->steal_data(ChildHiddenKey);
                gtr_widget_set_visible(*l, true);
            }
            else if (!b)
            {
                l->set_data(ChildHiddenKey, GINT_TO_POINTER(1));
                gtr_widget_set_visible(*l, false);

                // Retrieve updated top-levels list in case hiding the window resulted in its destruction
                top_levels = Gtk::Window::list_toplevels();
                top_level_it = top_levels.begin();
            }
        }
    }

    w.set_visible(b);
}

void gtr_window_set_skip_taskbar_hint([[maybe_unused]] Gtk::Window& window, [[maybe_unused]] bool value)
{
#if GTK_CHECK_VERSION(4, 0, 0)
#if defined(GDK_WINDOWING_X11)
    if (auto* const surface = Glib::unwrap(window.get_surface()); GDK_IS_X11_SURFACE(surface))
    {
        gdk_x11_surface_set_skip_taskbar_hint(surface, value ? TRUE : FALSE);
    }
#endif
#else
    window.set_skip_taskbar_hint(value);
#endif
}

void gtr_window_set_urgency_hint([[maybe_unused]] Gtk::Window& window, [[maybe_unused]] bool value)
{
#if GTK_CHECK_VERSION(4, 0, 0)
#if defined(GDK_WINDOWING_X11)
    if (auto* const surface = Glib::unwrap(window.get_surface()); GDK_IS_X11_SURFACE(surface))
    {
        gdk_x11_surface_set_urgency_hint(surface, value ? TRUE : FALSE);
    }
#endif
#else
    window.set_urgency_hint(value);
#endif
}

void gtr_window_raise([[maybe_unused]] Gtk::Window& window)
{
#if !GTKMM_CHECK_VERSION(4, 0, 0)
    window.get_window()->raise();
#endif
}

/***
****
***/

void gtr_unrecognized_url_dialog(Gtk::Widget& parent, Glib::ustring const& url)
{
    auto* window = getWindow(&parent);

    Glib::ustring gstr;

    auto w = std::make_shared<Gtk::MessageDialog>(
        *window,
        fmt::format(_("Unsupported URL: '{url}'"), fmt::arg("url", url)),
        false /*use markup*/,
        TR_GTK_MESSAGE_TYPE(ERROR),
        TR_GTK_BUTTONS_TYPE(CLOSE),
        true /*modal*/);

    gstr += fmt::format(_("Transmission doesn't know how to use '{url}'"), fmt::arg("url", url));

    if (tr_magnet_metainfo{}.parseMagnet(url.raw()))
    {
        gstr += "\n \n";
        gstr += _("This magnet link appears to be intended for something other than BitTorrent.");
    }

    w->set_secondary_text(gstr);
    w->signal_response().connect([w](int /*response*/) mutable { w.reset(); });
    w->show();
}

/***
****
***/

void gtr_paste_clipboard_url_into_entry(Gtk::Entry& entry)
{
    auto const process = [&entry](Glib::ustring const& text)
    {
        if (auto const sv = tr_strvStrip(text.raw());
            !sv.empty() && (tr_urlIsValid(sv) || tr_magnet_metainfo{}.parseMagnet(sv)))
        {
            entry.set_text(text);
            return true;
        }
        return false;
    };

#if GTKMM_CHECK_VERSION(4, 0, 0)
    auto const request = [](Glib::RefPtr<Gdk::Clipboard> const& clipboard, auto&& callback)
    {
        clipboard->read_text_async([clipboard, callback](Glib::RefPtr<Gio::AsyncResult>& result)
                                   { callback(clipboard->read_text_finish(result)); });
    };

    request(
        Gdk::Display::get_default()->get_primary_clipboard(),
        [request, process](Glib::ustring const& text)
        {
            if (!process(text))
            {
                request(Gdk::Display::get_default()->get_clipboard(), process);
            }
        });
#else
    for (auto const& str : { Gtk::Clipboard::get(GDK_SELECTION_PRIMARY)->wait_for_text(),
                             Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD)->wait_for_text() })
    {
        if (process(str))
        {
            break;
        }
    }
#endif
}

/***
****
***/

void gtr_label_set_text(Gtk::Label& lb, Glib::ustring const& text)
{
    if (lb.get_text() != text)
    {
        lb.set_text(text);
    }
}

std::string gtr_get_full_resource_path(std::string const& rel_path)
{
    static auto const BasePath = "/com/transmissionbt/transmission/"s;
    return BasePath + rel_path;
}

/***
****
***/

size_t const max_recent_dirs = size_t{ 4 };

std::list<std::string> gtr_get_recent_dirs(std::string const& pref)
{
    std::list<std::string> list;

    for (size_t i = 0; i < max_recent_dirs; ++i)
    {
        auto const key = fmt::format("recent-{}-dir-{}", pref, i + 1);

        if (auto const val = gtr_pref_string_get(tr_quark_new(key)); !val.empty())
        {
            list.push_back(val);
        }
    }

    return list;
}

void gtr_save_recent_dir(std::string const& pref, Glib::RefPtr<Session> const& core, std::string const& dir)
{
    if (dir.empty())
    {
        return;
    }

    auto list = gtr_get_recent_dirs(pref);

    /* if it was already in the list, remove it */
    list.remove(dir);

    /* add it to the front of the list */
    list.push_front(dir);

    /* save the first max_recent_dirs directories */
    list.resize(max_recent_dirs);
    int i = 0;
    for (auto const& d : list)
    {
        auto const key = fmt::format("recent-{}-dir-{}", pref, ++i);
        gtr_pref_string_set(tr_quark_new(key), d);
    }

    gtr_pref_save(core->get_session());
}
