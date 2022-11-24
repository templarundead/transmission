// This file Copyright © 2009-2022 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#else
#include <QTextCodec>
#endif

#include <libtransmission/transmission.h>
#include <libtransmission/utils.h>
#include <libtransmission/variant.h>

#include "CustomVariantType.h"
#include "Filters.h"
#include "Prefs.h"
#include "VariantHelpers.h"

using ::trqt::variant_helpers::dictAdd;
using ::trqt::variant_helpers::getValue;

/***
****
***/

namespace
{

void ensureSoundCommandIsAList(tr_variant* dict)
{
    tr_quark const key = TR_KEY_torrent_complete_sound_command;

    if (tr_variant* list = nullptr; tr_variantDictFindList(dict, key, &list))
    {
        return;
    }

    tr_variantDictRemove(dict, key);
    dictAdd(
        dict,
        key,
        std::array<std::string_view, 5>{
            "canberra-gtk-play",
            TR_ARG_TUPLE("-i", "complete-download"),
            TR_ARG_TUPLE("-d", "transmission torrent downloaded"),
        });
}

} // namespace

std::array<Prefs::PrefItem, Prefs::PREFS_COUNT> const Prefs::Items{
    /* gui settings */
    PrefItem{ OPTIONS_PROMPT, TR_KEY_show_options_window, QVariant::Bool },
    { OPEN_DIALOG_FOLDER, TR_KEY_open_dialog_dir, QVariant::String },
    { INHIBIT_HIBERNATION, TR_KEY_inhibit_desktop_hibernation, QVariant::Bool },
    { DIR_WATCH, TR_KEY_watch_dir, QVariant::String },
    { DIR_WATCH_ENABLED, TR_KEY_watch_dir_enabled, QVariant::Bool },
    { SHOW_TRAY_ICON, TR_KEY_show_notification_area_icon, QVariant::Bool },
    { START_MINIMIZED, TR_KEY_start_minimized, QVariant::Bool },
    { SHOW_NOTIFICATION_ON_ADD, TR_KEY_torrent_added_notification_enabled, QVariant::Bool },
    { SHOW_NOTIFICATION_ON_COMPLETE, TR_KEY_torrent_complete_notification_enabled, QVariant::Bool },
    { ASKQUIT, TR_KEY_prompt_before_exit, QVariant::Bool },
    { SORT_MODE, TR_KEY_sort_mode, CustomVariantType::SortModeType },
    { SORT_REVERSED, TR_KEY_sort_reversed, QVariant::Bool },
    { COMPACT_VIEW, TR_KEY_compact_view, QVariant::Bool },
    { FILTERBAR, TR_KEY_show_filterbar, QVariant::Bool },
    { STATUSBAR, TR_KEY_show_statusbar, QVariant::Bool },
    { STATUSBAR_STATS, TR_KEY_statusbar_stats, QVariant::String },
    { SHOW_TRACKER_SCRAPES, TR_KEY_show_extra_peer_details, QVariant::Bool },
    { SHOW_BACKUP_TRACKERS, TR_KEY_show_backup_trackers, QVariant::Bool },
    { TOOLBAR, TR_KEY_show_toolbar, QVariant::Bool },
    { BLOCKLIST_DATE, TR_KEY_blocklist_date, QVariant::DateTime },
    { BLOCKLIST_UPDATES_ENABLED, TR_KEY_blocklist_updates_enabled, QVariant::Bool },
    { MAIN_WINDOW_LAYOUT_ORDER, TR_KEY_main_window_layout_order, QVariant::String },
    { MAIN_WINDOW_HEIGHT, TR_KEY_main_window_height, QVariant::Int },
    { MAIN_WINDOW_WIDTH, TR_KEY_main_window_width, QVariant::Int },
    { MAIN_WINDOW_X, TR_KEY_main_window_x, QVariant::Int },
    { MAIN_WINDOW_Y, TR_KEY_main_window_y, QVariant::Int },
    { FILTER_MODE, TR_KEY_filter_mode, CustomVariantType::FilterModeType },
    { FILTER_TRACKERS, TR_KEY_filter_trackers, QVariant::String },
    { FILTER_TEXT, TR_KEY_filter_text, QVariant::String },
    { SESSION_IS_REMOTE, TR_KEY_remote_session_enabled, QVariant::Bool },
    { SESSION_REMOTE_HOST, TR_KEY_remote_session_host, QVariant::String },
    { SESSION_REMOTE_PORT, TR_KEY_remote_session_port, QVariant::Int },
    { SESSION_REMOTE_AUTH, TR_KEY_remote_session_requres_authentication, QVariant::Bool },
    { SESSION_REMOTE_USERNAME, TR_KEY_remote_session_username, QVariant::String },
    { SESSION_REMOTE_PASSWORD, TR_KEY_remote_session_password, QVariant::String },
    { COMPLETE_SOUND_COMMAND, TR_KEY_torrent_complete_sound_command, QVariant::StringList },
    { COMPLETE_SOUND_ENABLED, TR_KEY_torrent_complete_sound_enabled, QVariant::Bool },
    { USER_HAS_GIVEN_INFORMED_CONSENT, TR_KEY_user_has_given_informed_consent, QVariant::Bool },
    { READ_CLIPBOARD, TR_KEY_read_clipboard, QVariant::Bool },

    /* libtransmission settings */
    { ALT_SPEED_LIMIT_UP, TR_KEY_alt_speed_up, QVariant::Int },
    { ALT_SPEED_LIMIT_DOWN, TR_KEY_alt_speed_down, QVariant::Int },
    { ALT_SPEED_LIMIT_ENABLED, TR_KEY_alt_speed_enabled, QVariant::Bool },
    { ALT_SPEED_LIMIT_TIME_BEGIN, TR_KEY_alt_speed_time_begin, QVariant::Int },
    { ALT_SPEED_LIMIT_TIME_END, TR_KEY_alt_speed_time_end, QVariant::Int },
    { ALT_SPEED_LIMIT_TIME_ENABLED, TR_KEY_alt_speed_time_enabled, QVariant::Bool },
    { ALT_SPEED_LIMIT_TIME_DAY, TR_KEY_alt_speed_time_day, QVariant::Int },
    { BLOCKLIST_ENABLED, TR_KEY_blocklist_enabled, QVariant::Bool },
    { BLOCKLIST_URL, TR_KEY_blocklist_url, QVariant::String },
    { DEFAULT_TRACKERS, TR_KEY_default_trackers, QVariant::String },
    { DSPEED, TR_KEY_speed_limit_down, QVariant::Int },
    { DSPEED_ENABLED, TR_KEY_speed_limit_down_enabled, QVariant::Bool },
    { DOWNLOAD_DIR, TR_KEY_download_dir, QVariant::String },
    { DOWNLOAD_QUEUE_ENABLED, TR_KEY_download_queue_enabled, QVariant::Bool },
    { DOWNLOAD_QUEUE_SIZE, TR_KEY_download_queue_size, QVariant::Int },
    { ENCRYPTION, TR_KEY_encryption, QVariant::Int },
    { IDLE_LIMIT, TR_KEY_idle_seeding_limit, QVariant::Int },
    { IDLE_LIMIT_ENABLED, TR_KEY_idle_seeding_limit_enabled, QVariant::Bool },
    { INCOMPLETE_DIR, TR_KEY_incomplete_dir, QVariant::String },
    { INCOMPLETE_DIR_ENABLED, TR_KEY_incomplete_dir_enabled, QVariant::Bool },
    { MSGLEVEL, TR_KEY_message_level, QVariant::Int },
    { PEER_LIMIT_GLOBAL, TR_KEY_peer_limit_global, QVariant::Int },
    { PEER_LIMIT_TORRENT, TR_KEY_peer_limit_per_torrent, QVariant::Int },
    { PEER_PORT, TR_KEY_peer_port, QVariant::Int },
    { PEER_PORT_RANDOM_ON_START, TR_KEY_peer_port_random_on_start, QVariant::Bool },
    { PEER_PORT_RANDOM_LOW, TR_KEY_peer_port_random_low, QVariant::Int },
    { PEER_PORT_RANDOM_HIGH, TR_KEY_peer_port_random_high, QVariant::Int },
    { QUEUE_STALLED_MINUTES, TR_KEY_queue_stalled_minutes, QVariant::Int },
    { SCRIPT_TORRENT_DONE_ENABLED, TR_KEY_script_torrent_done_enabled, QVariant::Bool },
    { SCRIPT_TORRENT_DONE_FILENAME, TR_KEY_script_torrent_done_filename, QVariant::String },
    { SCRIPT_TORRENT_DONE_SEEDING_ENABLED, TR_KEY_script_torrent_done_seeding_enabled, QVariant::Bool },
    { SCRIPT_TORRENT_DONE_SEEDING_FILENAME, TR_KEY_script_torrent_done_seeding_filename, QVariant::String },
    { SOCKET_TOS, TR_KEY_peer_socket_tos, QVariant::String },
    { START, TR_KEY_start_added_torrents, QVariant::Bool },
    { TRASH_ORIGINAL, TR_KEY_trash_original_torrent_files, QVariant::Bool },
    { PEX_ENABLED, TR_KEY_pex_enabled, QVariant::Bool },
    { DHT_ENABLED, TR_KEY_dht_enabled, QVariant::Bool },
    { UTP_ENABLED, TR_KEY_utp_enabled, QVariant::Bool },
    { LPD_ENABLED, TR_KEY_lpd_enabled, QVariant::Bool },
    { PORT_FORWARDING, TR_KEY_port_forwarding_enabled, QVariant::Bool },
    { PREALLOCATION, TR_KEY_preallocation, QVariant::Int },
    { RATIO, TR_KEY_ratio_limit, QVariant::Double },
    { RATIO_ENABLED, TR_KEY_ratio_limit_enabled, QVariant::Bool },
    { RENAME_PARTIAL_FILES, TR_KEY_rename_partial_files, QVariant::Bool },
    { RPC_AUTH_REQUIRED, TR_KEY_rpc_authentication_required, QVariant::Bool },
    { RPC_ENABLED, TR_KEY_rpc_enabled, QVariant::Bool },
    { RPC_PASSWORD, TR_KEY_rpc_password, QVariant::String },
    { RPC_PORT, TR_KEY_rpc_port, QVariant::Int },
    { RPC_USERNAME, TR_KEY_rpc_username, QVariant::String },
    { RPC_WHITELIST_ENABLED, TR_KEY_rpc_whitelist_enabled, QVariant::Bool },
    { RPC_WHITELIST, TR_KEY_rpc_whitelist, QVariant::String },
    { USPEED_ENABLED, TR_KEY_speed_limit_up_enabled, QVariant::Bool },
    { USPEED, TR_KEY_speed_limit_up, QVariant::Int },
    { UPLOAD_SLOTS_PER_TORRENT, TR_KEY_upload_slots_per_torrent, QVariant::Int },
};

namespace
{

auto constexpr FilterModes = std::array<std::pair<int, std::string_view>, FilterMode::NUM_MODES>{ {
    { FilterMode::SHOW_ALL, "show-all" },
    { FilterMode::SHOW_ACTIVE, "show-active" },
    { FilterMode::SHOW_DOWNLOADING, "show-downloading" },
    { FilterMode::SHOW_SEEDING, "show-seeding" },
    { FilterMode::SHOW_PAUSED, "show-paused" },
    { FilterMode::SHOW_FINISHED, "show-finished" },
    { FilterMode::SHOW_VERIFYING, "show-verifying" },
    { FilterMode::SHOW_ERROR, "show-error" },
} };

auto constexpr SortModes = std::array<std::pair<int, std::string_view>, SortMode::NUM_MODES>{ {
    { SortMode::SORT_BY_NAME, "sort-by-name" },
    { SortMode::SORT_BY_ACTIVITY, "sort-by-activity" },
    { SortMode::SORT_BY_AGE, "sort-by-age" },
    { SortMode::SORT_BY_ETA, "sort-by-eta" },
    { SortMode::SORT_BY_PROGRESS, "sort-by-progress" },
    { SortMode::SORT_BY_QUEUE, "sort-by-queue" },
    { SortMode::SORT_BY_RATIO, "sort-by-ratio" },
    { SortMode::SORT_BY_SIZE, "sort-by-size" },
    { SortMode::SORT_BY_STATE, "sort-by-state" },
    { SortMode::SORT_BY_ID, "sort-by-id" },
} };

bool isValidUtf8(QByteArray const& byteArray)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    auto decoder = QStringDecoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    auto const text = QString(decoder.decode(byteArray));
    return !decoder.hasError() && !text.contains(QChar::ReplacementCharacter);

#else

    auto const* const codec = QTextCodec::codecForName("UTF-8");
    auto state = QTextCodec::ConverterState{};
    codec->toUnicode(byteArray.constData(), byteArray.size(), &state);
    return state.invalidChars == 0;

#endif
}

} // namespace

/***
****
***/

Prefs::Prefs(QString config_dir)
    : config_dir_(std::move(config_dir))
{
    static_assert(sizeof(Items) / sizeof(Items[0]) == PREFS_COUNT);

#ifndef NDEBUG
    for (int i = 0; i < PREFS_COUNT; ++i)
    {
        assert(Items[i].id == i);
    }

#endif

    // these are the prefs that don't get saved to settings.json
    // when the application exits.
    temporary_prefs_ << FILTER_TEXT;

    tr_variant top;
    tr_variantInitDict(&top, 0);
    initDefaults(&top);
    tr_sessionLoadSettings(&top, config_dir_.toUtf8().constData(), nullptr);
    ensureSoundCommandIsAList(&top);

    for (int i = 0; i < PREFS_COUNT; ++i)
    {
        tr_variant const* b = tr_variantDictFind(&top, Items[i].key);

        switch (Items[i].type)
        {
        case QVariant::Int:
            if (auto const value = getValue<int64_t>(b); value)
            {
                values_[i].setValue(*value);
            }
            break;

        case CustomVariantType::SortModeType:
            if (auto const value = getValue<std::string_view>(b); value)
            {
                auto const test = [&value](auto const& item)
                {
                    return item.second == *value;
                };
                // NOLINTNEXTLINE(readability-qualified-auto)
                auto const it = std::find_if(std::cbegin(SortModes), std::cend(SortModes), test);
                auto const& pair = it == std::end(SortModes) ? SortModes.front() : *it;
                values_[i] = QVariant::fromValue(SortMode(pair.first));
            }
            break;

        case CustomVariantType::FilterModeType:
            if (auto const value = getValue<std::string_view>(b); value)
            {
                auto const test = [&value](auto const& item)
                {
                    return item.second == *value;
                };
                // NOLINTNEXTLINE(readability-qualified-auto)
                auto const it = std::find_if(std::cbegin(FilterModes), std::cend(FilterModes), test);
                auto const& pair = it == std::end(FilterModes) ? FilterModes.front() : *it;
                values_[i] = QVariant::fromValue(FilterMode(pair.first));
            }
            break;

        case QVariant::String:
            if (auto const value = getValue<QString>(b); value)
            {
                values_[i].setValue(*value);
            }
            break;

        case QVariant::StringList:
            if (auto const value = getValue<QStringList>(b); value)
            {
                values_[i].setValue(*value);
            }
            break;

        case QVariant::Bool:
            if (auto const value = getValue<bool>(b); value)
            {
                values_[i].setValue(*value);
            }
            break;

        case QVariant::Double:
            if (auto const value = getValue<double>(b); value)
            {
                values_[i].setValue(*value);
            }
            break;

        case QVariant::DateTime:
            if (auto const value = getValue<time_t>(b); value)
            {
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
                values_[i].setValue(QDateTime::fromSecsSinceEpoch(*value));
#else
                values_[i].setValue(QDateTime::fromTime_t(*value));
#endif
            }
            break;

        default:
            assert(false && "unhandled type");
            break;
        }
    }

    tr_variantClear(&top);
}

Prefs::~Prefs()
{
    // make a dict from settings.json
    tr_variant current_settings;
    tr_variantInitDict(&current_settings, PREFS_COUNT);

    for (int i = 0; i < PREFS_COUNT; ++i)
    {
        if (temporary_prefs_.contains(i))
        {
            continue;
        }

        tr_quark const key = Items[i].key;
        QVariant const& val = values_[i];

        switch (Items[i].type)
        {
        case QVariant::Int:
            dictAdd(&current_settings, key, val.toInt());
            break;

        case CustomVariantType::SortModeType:
            {
                auto const mode = val.value<SortMode>().mode();
                auto const test = [&mode](auto const& item)
                {
                    return item.first == mode;
                };
                // NOLINTNEXTLINE(readability-qualified-auto)
                auto const it = std::find_if(std::cbegin(SortModes), std::cend(SortModes), test);
                auto const& pair = it == std::end(SortModes) ? SortModes.front() : *it;
                dictAdd(&current_settings, key, pair.second);
                break;
            }

        case CustomVariantType::FilterModeType:
            {
                auto const mode = val.value<FilterMode>().mode();
                auto const test = [&mode](auto const& item)
                {
                    return item.first == mode;
                };
                // NOLINTNEXTLINE(readability-qualified-auto)
                auto const it = std::find_if(std::cbegin(FilterModes), std::cend(FilterModes), test);
                auto const& pair = it == std::end(FilterModes) ? FilterModes.front() : *it;
                dictAdd(&current_settings, key, pair.second);
                break;
            }

        case QVariant::String:
            dictAdd(&current_settings, key, val.toString());
            break;

        case QVariant::StringList:
            dictAdd(&current_settings, key, val.toStringList());
            break;

        case QVariant::Bool:
            dictAdd(&current_settings, key, val.toBool());
            break;

        case QVariant::Double:
            dictAdd(&current_settings, key, val.toDouble());
            break;

        case QVariant::DateTime:
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
            dictAdd(&current_settings, key, int64_t{ val.toDateTime().toSecsSinceEpoch() });
#else
            dictAdd(&current_settings, key, val.toDateTime().toTime_t());
#endif
            break;

        default:
            assert(false && "unhandled type");
            break;
        }
    }

    // update settings.json with our settings
    tr_variant file_settings;
    QFile const file(QDir(config_dir_).absoluteFilePath(QStringLiteral("settings.json")));

    if (!tr_variantFromFile(&file_settings, TR_VARIANT_PARSE_JSON, file.fileName().toStdString(), nullptr))
    {
        tr_variantInitDict(&file_settings, PREFS_COUNT);
    }

    tr_variantMergeDicts(&file_settings, &current_settings);
    tr_variantToFile(&file_settings, TR_VARIANT_FMT_JSON, file.fileName().toStdString());
    tr_variantClear(&file_settings);

    // cleanup
    tr_variantClear(&current_settings);
}

/**
 * This is where we initialize the preferences file with the default values.
 * If you add a new preferences key, you /must/ add a default value here.
 */
void Prefs::initDefaults(tr_variant* d) const
{
    auto constexpr FilterMode = std::string_view{ "all" };
    auto constexpr SessionHost = std::string_view{ "localhost" };
    auto constexpr SessionPassword = std::string_view{};
    auto constexpr SessionUsername = std::string_view{};
    auto constexpr SortMode = std::string_view{ "sort-by-name" };
    auto constexpr StatsMode = std::string_view{ "total-ratio" };
    auto constexpr WindowLayout = std::string_view{ "menu,toolbar,filter,list,statusbar" };

    auto const download_dir = tr_getDefaultDownloadDir();

    tr_variantDictReserve(d, 38);
    dictAdd(d, TR_KEY_blocklist_updates_enabled, true);
    dictAdd(d, TR_KEY_compact_view, false);
    dictAdd(d, TR_KEY_inhibit_desktop_hibernation, false);
    dictAdd(d, TR_KEY_prompt_before_exit, true);
    dictAdd(d, TR_KEY_remote_session_enabled, false);
    dictAdd(d, TR_KEY_remote_session_requres_authentication, false);
    dictAdd(d, TR_KEY_show_backup_trackers, false);
    dictAdd(d, TR_KEY_show_extra_peer_details, false);
    dictAdd(d, TR_KEY_show_filterbar, true);
    dictAdd(d, TR_KEY_show_notification_area_icon, false);
    dictAdd(d, TR_KEY_start_minimized, false);
    dictAdd(d, TR_KEY_show_options_window, true);
    dictAdd(d, TR_KEY_show_statusbar, true);
    dictAdd(d, TR_KEY_show_toolbar, true);
    dictAdd(d, TR_KEY_show_tracker_scrapes, false);
    dictAdd(d, TR_KEY_sort_reversed, false);
    dictAdd(d, TR_KEY_torrent_added_notification_enabled, true);
    dictAdd(d, TR_KEY_torrent_complete_notification_enabled, true);
    dictAdd(d, TR_KEY_torrent_complete_sound_enabled, true);
    dictAdd(d, TR_KEY_user_has_given_informed_consent, false);
    dictAdd(d, TR_KEY_watch_dir_enabled, false);
    dictAdd(d, TR_KEY_blocklist_date, 0);
    dictAdd(d, TR_KEY_main_window_height, 500);
    dictAdd(d, TR_KEY_main_window_width, 300);
    dictAdd(d, TR_KEY_main_window_x, 50);
    dictAdd(d, TR_KEY_main_window_y, 50);
    dictAdd(d, TR_KEY_remote_session_port, TR_DEFAULT_RPC_PORT);
    dictAdd(d, TR_KEY_download_dir, download_dir);
    dictAdd(d, TR_KEY_filter_mode, FilterMode);
    dictAdd(d, TR_KEY_main_window_layout_order, WindowLayout);
    dictAdd(d, TR_KEY_open_dialog_dir, QDir::home().absolutePath());
    dictAdd(d, TR_KEY_remote_session_host, SessionHost);
    dictAdd(d, TR_KEY_remote_session_password, SessionPassword);
    dictAdd(d, TR_KEY_remote_session_username, SessionUsername);
    dictAdd(d, TR_KEY_sort_mode, SortMode);
    dictAdd(d, TR_KEY_statusbar_stats, StatsMode);
    dictAdd(d, TR_KEY_watch_dir, download_dir);
    dictAdd(d, TR_KEY_read_clipboard, false);
}

/***
****
***/

bool Prefs::getBool(int key) const
{
    assert(Items[key].type == QVariant::Bool);
    return values_[key].toBool();
}

QString Prefs::getString(int key) const
{
    assert(Items[key].type == QVariant::String);

    if (auto const b = values_[key].toByteArray(); isValidUtf8(b.constData()))
    {
        values_[key].setValue(QString::fromUtf8(b.constData()));
    }

    return values_[key].toString();
}

int Prefs::getInt(int key) const
{
    assert(Items[key].type == QVariant::Int);
    return values_[key].toInt();
}

double Prefs::getDouble(int key) const
{
    assert(Items[key].type == QVariant::Double);
    return values_[key].toDouble();
}

QDateTime Prefs::getDateTime(int key) const
{
    assert(Items[key].type == QVariant::DateTime);
    return values_[key].toDateTime();
}

/***
****
***/

void Prefs::toggleBool(int key)
{
    set(key, !getBool(key));
}
