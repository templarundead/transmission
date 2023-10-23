// This file Copyright © 2009-2023 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#ifndef __TRANSMISSION__
#error only libtransmission should #include this header.
#endif

#include <cstddef> // size_t
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "libtransmission/transmission.h"

#include "libtransmission/announce-list.h"
#include "libtransmission/bandwidth.h"
#include "libtransmission/bitfield.h"
#include "libtransmission/block-info.h"
#include "libtransmission/completion.h"
#include "libtransmission/crypto-utils.h"
#include "libtransmission/file-piece-map.h"
#include "libtransmission/interned-string.h"
#include "libtransmission/log.h"
#include "libtransmission/observable.h"
#include "libtransmission/session.h"
#include "libtransmission/torrent-magnet.h"
#include "libtransmission/torrent-metainfo.h"
#include "libtransmission/tr-assert.h"
#include "libtransmission/tr-macros.h"
#include "libtransmission/verify.h"

class tr_swarm;
struct tr_error;
struct tr_magnet_info;
struct tr_metainfo_parsed;
struct tr_session;
struct tr_torrent;
struct tr_torrent_announcer;

// --- Package-visible

void tr_torrentFreeInSessionThread(tr_torrent* tor);

void tr_ctorInitTorrentPriorities(tr_ctor const* ctor, tr_torrent* tor);

void tr_ctorInitTorrentWanted(tr_ctor const* ctor, tr_torrent* tor);

bool tr_ctorSaveContents(tr_ctor const* ctor, std::string_view filename, tr_error** error);

tr_session* tr_ctorGetSession(tr_ctor const* ctor);

bool tr_ctorGetIncompleteDir(tr_ctor const* ctor, char const** setme_incomplete_dir);

// ---

void tr_torrentChangeMyPort(tr_torrent* tor);

[[nodiscard]] tr_torrent* tr_torrentFindFromObfuscatedHash(tr_session* session, tr_sha1_digest_t const& hash);

bool tr_torrentReqIsValid(tr_torrent const* tor, tr_piece_index_t index, uint32_t offset, uint32_t length);

[[nodiscard]] tr_block_span_t tr_torGetFileBlockSpan(tr_torrent const* tor, tr_file_index_t file);

void tr_torrentCheckSeedLimit(tr_torrent* tor);

/** save a torrent's .resume file if it's changed since the last time it was saved */
void tr_torrentSave(tr_torrent* tor);

/** @brief Torrent object */
struct tr_torrent final : public tr_completion::torrent_view
{
public:
    using VerifyDoneCallback = std::function<void(tr_torrent*)>;

    class VerifyMediator : public tr_verify_worker::Mediator
    {
    public:
        explicit VerifyMediator(tr_torrent* const tor)
            : tor_{ tor }
        {
        }

        ~VerifyMediator() override = default;

        [[nodiscard]] tr_torrent_metainfo const& metainfo() const override;
        [[nodiscard]] std::optional<std::string> find_file(tr_file_index_t file_index) const override;

        void on_verify_queued() override;
        void on_verify_started() override;
        void on_piece_checked(tr_piece_index_t piece, bool has_piece) override;
        void on_verify_done(bool aborted) override;

    private:
        tr_torrent* const tor_;
        std::optional<time_t> time_started_;
    };

    explicit tr_torrent(tr_torrent_metainfo&& tm)
        : metainfo_{ std::move(tm) }
        , completion{ this, &this->metainfo_.block_info() }
    {
    }

    void set_location(std::string_view location, bool move_from_old_path, int volatile* setme_state);

    void rename_path(
        std::string_view oldpath,
        std::string_view newname,
        tr_torrent_rename_done_func callback,
        void* callback_user_data);

    tr_sha1_digest_t piece_hash(tr_piece_index_t i) const
    {
        return metainfo_.piece_hash(i);
    }

    // these functions should become private when possible,
    // but more refactoring is needed before that can happen
    // because much of tr_torrent's impl is in the non-member C bindings

    // Used to add metainfo to a magnet torrent.
    void set_metainfo(tr_torrent_metainfo tm);

    [[nodiscard]] auto unique_lock() const
    {
        return session->unique_lock();
    }

    /// SPEED LIMIT

    [[nodiscard]] constexpr auto& bandwidth() noexcept
    {
        return bandwidth_;
    }

    [[nodiscard]] constexpr auto const& bandwidth() const noexcept
    {
        return bandwidth_;
    }

    constexpr void set_speed_limit_bps(tr_direction dir, tr_bytes_per_second_t bytes_per_second)
    {
        if (bandwidth().set_desired_speed_bytes_per_second(dir, bytes_per_second))
        {
            set_dirty();
        }
    }

    constexpr void use_speed_limit(tr_direction dir, bool do_use)
    {
        if (bandwidth().set_limited(dir, do_use))
        {
            set_dirty();
        }
    }

    [[nodiscard]] constexpr auto speed_limit_bps(tr_direction dir) const
    {
        return bandwidth().get_desired_speed_bytes_per_second(dir);
    }

    [[nodiscard]] constexpr auto uses_session_limits() const noexcept
    {
        return bandwidth().are_parent_limits_honored(TR_UP);
    }

    [[nodiscard]] constexpr auto uses_speed_limit(tr_direction dir) const noexcept
    {
        return bandwidth().is_limited(dir);
    }

    /// BLOCK INFO

    [[nodiscard]] constexpr auto const& block_info() const noexcept
    {
        return metainfo_.block_info();
    }

    [[nodiscard]] constexpr auto block_count() const noexcept
    {
        return metainfo_.block_count();
    }
    [[nodiscard]] constexpr auto byte_loc(uint64_t byte) const noexcept
    {
        return metainfo_.byte_loc(byte);
    }
    [[nodiscard]] constexpr auto block_loc(tr_block_index_t block) const noexcept
    {
        return metainfo_.block_loc(block);
    }
    [[nodiscard]] constexpr auto piece_loc(tr_piece_index_t piece, uint32_t offset = 0, uint32_t length = 0) const noexcept
    {
        return metainfo_.piece_loc(piece, offset, length);
    }
    [[nodiscard]] constexpr auto block_size(tr_block_index_t block) const noexcept
    {
        return metainfo_.block_size(block);
    }
    [[nodiscard]] constexpr auto block_span_for_piece(tr_piece_index_t piece) const noexcept
    {
        return metainfo_.block_span_for_piece(piece);
    }
    [[nodiscard]] constexpr auto piece_count() const noexcept
    {
        return metainfo_.piece_count();
    }
    [[nodiscard]] constexpr auto piece_size() const noexcept
    {
        return metainfo_.piece_size();
    }
    [[nodiscard]] constexpr auto piece_size(tr_piece_index_t piece) const noexcept
    {
        return metainfo_.piece_size(piece);
    }
    [[nodiscard]] constexpr auto total_size() const noexcept
    {
        return metainfo_.total_size();
    }

    /// COMPLETION

    [[nodiscard]] auto left_until_done() const
    {
        return completion.left_until_done();
    }

    [[nodiscard]] auto size_when_done() const
    {
        return completion.size_when_done();
    }

    [[nodiscard]] constexpr auto has_metainfo() const noexcept
    {
        return completion.has_metainfo();
    }

    [[nodiscard]] constexpr auto has_all() const noexcept
    {
        return completion.has_all();
    }

    [[nodiscard]] constexpr auto has_none() const noexcept
    {
        return completion.has_none();
    }

    [[nodiscard]] auto has_piece(tr_piece_index_t piece) const
    {
        return completion.has_piece(piece);
    }

    [[nodiscard]] TR_CONSTEXPR20 auto has_block(tr_block_index_t block) const
    {
        return completion.has_block(block);
    }

    [[nodiscard]] auto count_missing_blocks_in_piece(tr_piece_index_t piece) const
    {
        return completion.count_missing_blocks_in_piece(piece);
    }

    [[nodiscard]] auto count_missing_bytes_in_piece(tr_piece_index_t piece) const
    {
        return completion.count_missing_bytes_in_piece(piece);
    }

    [[nodiscard]] constexpr auto has_total() const
    {
        return completion.has_total();
    }

    [[nodiscard]] auto create_piece_bitfield() const
    {
        return completion.create_piece_bitfield();
    }

    [[nodiscard]] constexpr bool is_done() const noexcept
    {
        return completeness != TR_LEECH;
    }

    [[nodiscard]] constexpr bool is_seed() const noexcept
    {
        return completeness == TR_SEED;
    }

    [[nodiscard]] constexpr bool is_partial_seed() const noexcept
    {
        return completeness == TR_PARTIAL_SEED;
    }

    [[nodiscard]] constexpr auto& blocks() const noexcept
    {
        return completion.blocks();
    }

    void amount_done_bins(float* tab, int n_tabs) const
    {
        return completion.amount_done(tab, n_tabs);
    }

    void set_blocks(tr_bitfield blocks);

    void set_has_piece(tr_piece_index_t piece, bool has)
    {
        completion.set_has_piece(piece, has);
    }

    /// FILE <-> PIECE

    [[nodiscard]] auto pieces_in_file(tr_file_index_t file) const
    {
        return fpm_.piece_span(file);
    }

    [[nodiscard]] auto file_offset(tr_block_info::Location loc) const
    {
        return fpm_.file_offset(loc.byte);
    }

    [[nodiscard]] auto byte_span(tr_file_index_t file) const
    {
        return fpm_.byte_span(file);
    }

    /// WANTED

    [[nodiscard]] bool piece_is_wanted(tr_piece_index_t piece) const final
    {
        return files_wanted_.piece_wanted(piece);
    }

    [[nodiscard]] TR_CONSTEXPR20 bool file_is_wanted(tr_file_index_t file) const
    {
        return files_wanted_.file_wanted(file);
    }

    void init_files_wanted(tr_file_index_t const* files, size_t n_files, bool wanted)
    {
        set_files_wanted(files, n_files, wanted, /*is_bootstrapping*/ true);
    }

    void set_files_wanted(tr_file_index_t const* files, size_t n_files, bool wanted)
    {
        set_files_wanted(files, n_files, wanted, /*is_bootstrapping*/ false);
    }

    void recheck_completeness(); // TODO(ckerr): should be private

    /// PRIORITIES

    [[nodiscard]] tr_priority_t piece_priority(tr_piece_index_t piece) const
    {
        return file_priorities_.piece_priority(piece);
    }

    void set_file_priorities(tr_file_index_t const* files, tr_file_index_t file_count, tr_priority_t priority)
    {
        file_priorities_.set(files, file_count, priority);
        set_dirty();
    }

    void set_file_priority(tr_file_index_t file, tr_priority_t priority)
    {
        file_priorities_.set(file, priority);
        set_dirty();
    }

    /// LOCATION

    [[nodiscard]] constexpr tr_interned_string current_dir() const noexcept
    {
        return current_dir_;
    }

    [[nodiscard]] constexpr tr_interned_string download_dir() const noexcept
    {
        return download_dir_;
    }

    [[nodiscard]] constexpr tr_interned_string incomplete_dir() const noexcept
    {
        return incomplete_dir_;
    }

    /// METAINFO - FILES

    [[nodiscard]] TR_CONSTEXPR20 auto file_count() const noexcept
    {
        return metainfo_.file_count();
    }

    [[nodiscard]] TR_CONSTEXPR20 auto const& file_subpath(tr_file_index_t i) const
    {
        return metainfo_.file_subpath(i);
    }

    [[nodiscard]] TR_CONSTEXPR20 auto file_size(tr_file_index_t i) const
    {
        return metainfo_.file_size(i);
    }

    void set_file_subpath(tr_file_index_t i, std::string_view subpath)
    {
        metainfo_.set_file_subpath(i, subpath);
    }

    [[nodiscard]] std::optional<tr_torrent_files::FoundFile> find_file(tr_file_index_t file_index) const;

    [[nodiscard]] bool has_any_local_data() const;

    /// METAINFO - TRACKERS

    [[nodiscard]] constexpr auto const& announce_list() const noexcept
    {
        return metainfo_.announce_list();
    }

    [[nodiscard]] constexpr auto& announce_list() noexcept
    {
        return metainfo_.announce_list();
    }

    [[nodiscard]] TR_CONSTEXPR20 auto tracker_count() const noexcept
    {
        return std::size(this->announce_list());
    }

    [[nodiscard]] TR_CONSTEXPR20 auto const& tracker(size_t i) const
    {
        return this->announce_list().at(i);
    }

    [[nodiscard]] auto tracker_list() const
    {
        return this->announce_list().to_string();
    }

    bool set_tracker_list(std::string_view text);

    void on_tracker_response(tr_tracker_event const* event);

    /// METAINFO - WEBSEEDS

    [[nodiscard]] TR_CONSTEXPR20 auto webseed_count() const noexcept
    {
        return metainfo_.webseed_count();
    }

    [[nodiscard]] TR_CONSTEXPR20 auto const& webseed(size_t i) const
    {
        return metainfo_.webseed(i);
    }

    /// METAINFO - OTHER

    void set_name(std::string_view name)
    {
        metainfo_.set_name(name);
    }

    [[nodiscard]] constexpr auto const& name() const noexcept
    {
        return metainfo_.name();
    }

    [[nodiscard]] constexpr auto const& info_hash() const noexcept
    {
        return metainfo_.info_hash();
    }

    [[nodiscard]] constexpr auto is_private() const noexcept
    {
        return metainfo_.is_private();
    }

    [[nodiscard]] constexpr auto is_public() const noexcept
    {
        return !this->is_private();
    }

    [[nodiscard]] constexpr auto const& info_hash_string() const noexcept
    {
        return metainfo_.info_hash_string();
    }

    [[nodiscard]] constexpr auto date_created() const noexcept
    {
        return metainfo_.date_created();
    }

    [[nodiscard]] auto torrent_file() const
    {
        return metainfo_.torrent_file(session->torrentDir());
    }

    [[nodiscard]] auto magnet_file() const
    {
        return metainfo_.magnet_file(session->torrentDir());
    }

    [[nodiscard]] auto resume_file() const
    {
        return metainfo_.resume_file(session->resumeDir());
    }

    [[nodiscard]] auto magnet() const
    {
        return metainfo_.magnet();
    }

    [[nodiscard]] constexpr auto const& comment() const noexcept
    {
        return metainfo_.comment();
    }

    [[nodiscard]] constexpr auto const& creator() const noexcept
    {
        return metainfo_.creator();
    }

    [[nodiscard]] constexpr auto const& source() const noexcept
    {
        return metainfo_.source();
    }

    [[nodiscard]] constexpr auto info_dict_size() const noexcept
    {
        return metainfo_.info_dict_size();
    }

    [[nodiscard]] constexpr auto info_dict_offset() const noexcept
    {
        return metainfo_.info_dict_offset();
    }

    /// METAINFO - PIECE CHECKSUMS

    [[nodiscard]] TR_CONSTEXPR20 bool is_piece_checked(tr_piece_index_t piece) const
    {
        return checked_pieces_.test(piece);
    }

    [[nodiscard]] bool check_piece(tr_piece_index_t piece);

    [[nodiscard]] bool ensure_piece_is_checked(tr_piece_index_t piece);

    void init_checked_pieces(tr_bitfield const& checked, time_t const* mtimes /*fileCount()*/);

    ///

    [[nodiscard]] tr_stat stats() const;

    [[nodiscard]] constexpr auto is_queued() const noexcept
    {
        return this->is_queued_;
    }

    [[nodiscard]] constexpr auto queue_direction() const noexcept
    {
        return this->is_done() ? TR_UP : TR_DOWN;
    }

    [[nodiscard]] constexpr auto allows_pex() const noexcept
    {
        return this->is_public() && this->session->allows_pex();
    }

    [[nodiscard]] constexpr auto allows_dht() const noexcept
    {
        return this->is_public() && this->session->allowsDHT();
    }

    [[nodiscard]] constexpr auto allows_lpd() const noexcept // local peer discovery
    {
        return this->is_public() && this->session->allowsLPD();
    }

    [[nodiscard]] constexpr bool client_can_download() const
    {
        return this->is_piece_transfer_allowed(TR_PEER_TO_CLIENT);
    }

    [[nodiscard]] constexpr bool client_can_upload() const
    {
        return this->is_piece_transfer_allowed(TR_CLIENT_TO_PEER);
    }

    void set_download_dir(std::string_view path, bool is_new_torrent = false);

    void refresh_current_dir();

    [[nodiscard]] constexpr std::optional<float> verify_progress() const noexcept
    {
        if (verify_state_ == VerifyState::Active)
        {
            return verify_progress_;
        }

        return {};
    }

    [[nodiscard]] constexpr auto id() const noexcept
    {
        return unique_id_;
    }

    void init_id(tr_torrent_id_t id)
    {
        TR_ASSERT(unique_id_ == tr_torrent_id_t{});
        TR_ASSERT(id != tr_torrent_id_t{});
        unique_id_ = id;
    }

    constexpr void set_date_active(time_t when) noexcept
    {
        this->activityDate = when;

        bump_date_changed(when);
    }

    [[nodiscard]] constexpr auto activity() const noexcept
    {
        bool const is_seed = this->is_done();

        if (verify_state_ == VerifyState::Active)
        {
            return TR_STATUS_CHECK;
        }

        if (verify_state_ == VerifyState::Queued)
        {
            return TR_STATUS_CHECK_WAIT;
        }

        if (this->is_running())
        {
            return is_seed ? TR_STATUS_SEED : TR_STATUS_DOWNLOAD;
        }

        if (this->is_queued())
        {
            if (is_seed && this->session->queueEnabled(TR_UP))
            {
                return TR_STATUS_SEED_WAIT;
            }

            if (!is_seed && this->session->queueEnabled(TR_DOWN))
            {
                return TR_STATUS_DOWNLOAD_WAIT;
            }
        }

        return TR_STATUS_STOPPED;
    }

    void setLabels(std::vector<tr_quark> const& new_labels);

    /** Return the mime-type (e.g. "audio/x-flac") that matches more of the
        torrent's content than any other mime-type. */
    [[nodiscard]] std::string_view primary_mime_type() const;

    constexpr void set_sequential_download(bool is_sequential) noexcept
    {
        sequential_download_ = is_sequential;
    }

    [[nodiscard]] constexpr auto is_sequential_download() const noexcept
    {
        return sequential_download_;
    }

    [[nodiscard]] constexpr bool is_running() const noexcept
    {
        return is_running_;
    }

    [[nodiscard]] constexpr auto is_stopping() const noexcept
    {
        return is_stopping_;
    }

    [[nodiscard]] constexpr auto is_dirty() const noexcept
    {
        return is_dirty_;
    }

    constexpr void set_dirty(bool dirty = true) noexcept
    {
        is_dirty_ = dirty;
    }

    void mark_edited();
    void mark_changed();

    [[nodiscard]] constexpr auto has_changed_since(time_t when) const noexcept
    {
        return changed_date_ > when;
    }

    void set_bandwidth_group(std::string_view group_name) noexcept;

    [[nodiscard]] constexpr auto get_priority() const noexcept
    {
        return bandwidth().get_priority();
    }

    [[nodiscard]] constexpr auto const& bandwidth_group() const noexcept
    {
        return bandwidth_group_;
    }

    [[nodiscard]] constexpr auto peer_limit() const noexcept
    {
        return max_connected_peers_;
    }

    // --- idleness

    void set_idle_limit_mode(tr_idlelimit mode) noexcept
    {
        auto const is_valid = mode == TR_IDLELIMIT_GLOBAL || mode == TR_IDLELIMIT_SINGLE || mode == TR_IDLELIMIT_UNLIMITED;
        TR_ASSERT(is_valid);
        if (idle_limit_mode_ != mode && is_valid)
        {
            idle_limit_mode_ = mode;
            set_dirty();
        }
    }

    [[nodiscard]] constexpr auto idle_limit_mode() const noexcept
    {
        return idle_limit_mode_;
    }

    constexpr void set_idle_limit_minutes(uint16_t idle_minutes) noexcept
    {
        if ((idle_limit_minutes_ != idle_minutes) && (idle_minutes > 0))
        {
            idle_limit_minutes_ = idle_minutes;
            set_dirty();
        }
    }

    [[nodiscard]] constexpr auto idle_limit_minutes() const noexcept
    {
        return idle_limit_minutes_;
    }

    [[nodiscard]] constexpr std::optional<size_t> idle_seconds_left(time_t now) const noexcept
    {
        auto const idle_limit_minutes = effective_idle_limit_minutes();
        if (!idle_limit_minutes)
        {
            return {};
        }

        auto const idle_seconds = this->idle_seconds(now);
        if (!idle_seconds)
        {
            return {};
        }

        auto const idle_limit_seconds = size_t{ *idle_limit_minutes } * 60U;
        return idle_limit_seconds > *idle_seconds ? idle_limit_seconds - *idle_seconds : 0U;
    }

    // --- seed ratio

    constexpr void set_seed_ratio_mode(tr_ratiolimit mode) noexcept
    {
        auto const is_valid = mode == TR_RATIOLIMIT_GLOBAL || mode == TR_RATIOLIMIT_SINGLE || mode == TR_RATIOLIMIT_UNLIMITED;
        TR_ASSERT(is_valid);
        if (seed_ratio_mode_ != mode && is_valid)
        {
            seed_ratio_mode_ = mode;
            set_dirty();
        }
    }

    [[nodiscard]] constexpr auto seed_ratio_mode() const noexcept
    {
        return seed_ratio_mode_;
    }

    constexpr void set_seed_ratio(double desired_ratio)
    {
        if (static_cast<int>(seed_ratio_ * 100.0) != static_cast<int>(desired_ratio * 100.0))
        {
            seed_ratio_ = desired_ratio;
            set_dirty();
        }
    }

    [[nodiscard]] auto seed_ratio() const noexcept
    {
        return seed_ratio_;
    }

    [[nodiscard]] constexpr std::optional<double> effective_seed_ratio() const noexcept
    {
        auto const mode = seed_ratio_mode();

        if (mode == TR_RATIOLIMIT_SINGLE)
        {
            return seed_ratio_;
        }

        if (mode == TR_RATIOLIMIT_GLOBAL && session->isRatioLimited())
        {
            return session->desiredRatio();
        }

        return {};
    }

    // ---

    [[nodiscard]] constexpr auto seconds_downloading(time_t now) const noexcept
    {
        auto n_secs = seconds_downloading_before_current_start_;

        if (is_running())
        {
            if (doneDate > startDate)
            {
                n_secs += doneDate - startDate;
            }
            else if (doneDate == 0)
            {
                n_secs += now - startDate;
            }
        }

        return n_secs;
    }

    [[nodiscard]] constexpr auto seconds_seeding(time_t now) const noexcept
    {
        auto n_secs = seconds_seeding_before_current_start_;

        if (is_running())
        {
            if (doneDate > startDate)
            {
                n_secs += now - doneDate;
            }
            else if (doneDate != 0)
            {
                n_secs += now - startDate;
            }
        }

        return n_secs;
    }

    constexpr void set_needs_completeness_check() noexcept
    {
        needs_completeness_check_ = true;
    }

    void do_idle_work()
    {
        if (needs_completeness_check_)
        {
            needs_completeness_check_ = false;
            recheck_completeness();
        }

        if (is_stopping_)
        {
            tr_torrentStop(this);
        }
    }

    [[nodiscard]] constexpr auto announce_key() const noexcept
    {
        return announce_key_;
    }

    [[nodiscard]] constexpr tr_peer_id_t const& peer_id() const noexcept
    {
        return peer_id_;
    }

    // should be called when done modifying the torrent's announce list.
    void on_announce_list_changed()
    {
        mark_edited();
        session->announcer_->resetTorrent(this);
    }

    [[nodiscard]] constexpr auto& error() noexcept
    {
        return error_;
    }

    [[nodiscard]] constexpr auto const& error() const noexcept
    {
        return error_;
    }

    void init(tr_ctor const* ctor);

    tr_torrent_metainfo metainfo_;

    tr_bandwidth bandwidth_;

    libtransmission::SimpleObservable<tr_torrent*, bool /*because_downloaded_last_piece*/> done_;
    libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t> got_bad_piece_;
    libtransmission::SimpleObservable<tr_torrent*, tr_piece_index_t> piece_completed_;
    libtransmission::SimpleObservable<tr_torrent*> doomed_;
    libtransmission::SimpleObservable<tr_torrent*> got_metainfo_;
    libtransmission::SimpleObservable<tr_torrent*> started_;
    libtransmission::SimpleObservable<tr_torrent*> stopped_;
    libtransmission::SimpleObservable<tr_torrent*> swarm_is_all_seeds_;

    // TODO(ckerr): make private once some of torrent.cc's `tr_torrentFoo()` methods are member functions
    tr_completion completion;

    // true iff the piece was verified more recently than any of the piece's
    // files' mtimes (file_mtimes_). If checked_pieces_.test(piece) is false,
    // it means that piece needs to be checked before its data is used.
    tr_bitfield checked_pieces_ = tr_bitfield{ 0 };

    tr_file_piece_map fpm_ = tr_file_piece_map{ metainfo_ };

    using labels_t = std::vector<tr_quark>;
    labels_t labels;

    // when Transmission thinks the torrent's files were last changed
    std::vector<time_t> file_mtimes_;

    // Where the files are when the torrent is complete.
    tr_interned_string download_dir_;

    // Where the files are when the torrent is incomplete.
    // a value of TR_KEY_NONE indicates the 'incomplete_dir' feature is unused
    tr_interned_string incomplete_dir_;

    // Where the files are now.
    // Will equal either download_dir or incomplete_dir
    tr_interned_string current_dir_;

    tr_sha1_digest_t obfuscated_hash = {};

    /* Used when the torrent has been created with a magnet link
     * and we're in the process of downloading the metainfo from
     * other peers */
    std::unique_ptr<tr_incomplete_metadata> incomplete_metadata;

    tr_session* session = nullptr;

    tr_torrent_announcer* torrent_announcer = nullptr;

    tr_swarm* swarm = nullptr;

    time_t lpdAnnounceAt = 0;

    time_t activityDate = 0;
    time_t addedDate = 0;
    time_t doneDate = 0;
    time_t editDate = 0;
    time_t startDate = 0;

    time_t seconds_downloading_before_current_start_ = 0;
    time_t seconds_seeding_before_current_start_ = 0;

    uint64_t downloadedCur = 0;
    uint64_t downloadedPrev = 0;
    uint64_t uploadedCur = 0;
    uint64_t uploadedPrev = 0;
    uint64_t corruptCur = 0;
    uint64_t corruptPrev = 0;

    size_t queuePosition = 0;

    tr_completeness completeness = TR_LEECH;

    uint16_t max_connected_peers_ = TR_DEFAULT_PEER_LIMIT_TORRENT;

    bool finished_seeding_by_idle_ = false;

    bool is_deleting_ = false;
    bool is_dirty_ = false;
    bool is_queued_ = false;
    bool is_running_ = false;
    bool is_stopping_ = false;

    // start the torrent after all the startup scaffolding is done,
    // e.g. fetching metadata from peers and/or verifying the torrent
    bool start_when_stable = false;

private:
    friend tr_file_view tr_torrentFile(tr_torrent const* tor, tr_file_index_t file);
    friend tr_stat const* tr_torrentStat(tr_torrent* tor);
    friend tr_torrent* tr_torrentNew(tr_ctor* ctor, tr_torrent** setme_duplicate_of);
    friend uint64_t tr_torrentGetBytesLeftToAllocate(tr_torrent const* tor);
    friend uint64_t tr_torrentGetBytesLeftToAllocate(tr_torrent const* tor);

    enum class VerifyState : uint8_t
    {
        None,
        Queued,
        Active
    };

    // Tracks a torrent's error state, either local (e.g. file IO errors)
    // or tracker errors (e.g. warnings returned by a tracker).
    class Error
    {
    public:
        [[nodiscard]] constexpr auto empty() const noexcept
        {
            return error_type_ == TR_STAT_OK;
        }

        [[nodiscard]] constexpr auto error_type() const noexcept
        {
            return error_type_;
        }

        [[nodiscard]] constexpr auto const& announce_url() const noexcept
        {
            return announce_url_;
        }

        [[nodiscard]] constexpr auto const& errmsg() const noexcept
        {
            return errmsg_;
        }

        void set_tracker_warning(tr_interned_string announce_url, std::string_view errmsg);
        void set_tracker_error(tr_interned_string announce_url, std::string_view errmsg);
        void set_local_error(std::string_view errmsg);

        void clear() noexcept;
        void clear_if_tracker() noexcept;

    private:
        tr_interned_string announce_url_; // the source for tracker errors/warnings
        std::string errmsg_;
        tr_stat_errtype error_type_ = TR_STAT_OK;
    };

    // Helper class to smooth out speed estimates.
    // Used to prevent temporary speed changes from skewing the ETA too much.
    class SimpleSmoothedSpeed
    {
    public:
        constexpr auto update(uint64_t time_msec, tr_bytes_per_second_t speed_byps)
        {
            // If the old speed is too old, just replace it
            if (timestamp_msec_ + MaxAgeMSec <= time_msec)
            {
                timestamp_msec_ = time_msec;
                speed_byps_ = speed_byps;
            }

            // To prevent the smoothing from being overwhelmed by frequent calls
            // to update(), do nothing if not enough time elapsed since last update.
            else if (timestamp_msec_ + MinUpdateMSec <= time_msec)
            {
                timestamp_msec_ = time_msec;
                speed_byps_ = (speed_byps_ * 4U + speed_byps) / 5U;
            }

            return speed_byps_;
        }

    private:
        static auto constexpr MaxAgeMSec = 4000U;
        static auto constexpr MinUpdateMSec = 800U;

        uint64_t timestamp_msec_ = {};
        tr_bytes_per_second_t speed_byps_ = {};
    };

    [[nodiscard]] constexpr std::optional<uint16_t> effective_idle_limit_minutes() const noexcept
    {
        auto const mode = idle_limit_mode();

        if (mode == TR_IDLELIMIT_SINGLE)
        {
            return idle_limit_minutes();
        }

        if (mode == TR_IDLELIMIT_GLOBAL && session->isIdleLimited())
        {
            return session->idleLimitMinutes();
        }

        return {};
    }

    [[nodiscard]] constexpr std::optional<size_t> idle_seconds(time_t now) const noexcept
    {
        auto const activity = this->activity();

        if (activity == TR_STATUS_DOWNLOAD || activity == TR_STATUS_SEED)
        {
            if (auto const latest = std::max(startDate, activityDate); latest != 0)
            {
                TR_ASSERT(now >= latest);
                return now - latest;
            }
        }

        return {};
    }

    [[nodiscard]] constexpr bool is_piece_transfer_allowed(tr_direction direction) const noexcept
    {
        if (uses_speed_limit(direction) && speed_limit_bps(direction) <= 0)
        {
            return false;
        }

        if (uses_session_limits())
        {
            if (auto const limit = session->activeSpeedLimitBps(direction); limit && *limit == 0U)
            {
                return false;
            }
        }

        return true;
    }

    void set_files_wanted(tr_file_index_t const* files, size_t n_files, bool wanted, bool is_bootstrapping)
    {
        auto const lock = unique_lock();

        files_wanted_.set(files, n_files, wanted);
        completion.invalidate_size_when_done();

        if (!is_bootstrapping)
        {
            set_dirty();
            recheck_completeness();
        }
    }

    constexpr void bump_date_changed(time_t when)
    {
        if (changed_date_ < when)
        {
            changed_date_ = when;
        }
    }

    void set_verify_state(VerifyState state);

    void on_metainfo_updated();

    tr_stat stats_ = {};

    Error error_;

    VerifyDoneCallback verify_done_callback_;

    tr_interned_string bandwidth_group_;

    mutable SimpleSmoothedSpeed eta_speed_;

    tr_files_wanted files_wanted_{ &fpm_ };
    tr_file_priorities file_priorities_{ &fpm_ };

    /* If the initiator of the connection receives a handshake in which the
     * peer_id does not match the expected peerid, then the initiator is
     * expected to drop the connection. Note that the initiator presumably
     * received the peer information from the tracker, which includes the
     * peer_id that was registered by the peer. The peer_id from the tracker
     * and in the handshake are expected to match.
     */
    tr_peer_id_t peer_id_ = tr_peerIdInit();

    time_t changed_date_ = 0;

    float verify_progress_ = -1.0F;
    float seed_ratio_ = 0.0F;

    tr_announce_key_t announce_key_ = tr_rand_obj<tr_announce_key_t>();

    tr_torrent_id_t unique_id_ = 0;

    tr_ratiolimit seed_ratio_mode_ = TR_RATIOLIMIT_GLOBAL;

    tr_idlelimit idle_limit_mode_ = TR_IDLELIMIT_GLOBAL;

    VerifyState verify_state_ = VerifyState::None;

    uint16_t idle_limit_minutes_ = 0;

    bool needs_completeness_check_ = true;

    bool sequential_download_ = false;
};

// ---

constexpr bool tr_isTorrent(tr_torrent const* tor)
{
    return tor != nullptr && tor->session != nullptr;
}

/**
 * Tell the `tr_torrent` that it's gotten a block
 */
void tr_torrentGotBlock(tr_torrent* tor, tr_block_index_t block);

tr_torrent_metainfo tr_ctorStealMetainfo(tr_ctor* ctor);

bool tr_ctorSetMetainfoFromFile(tr_ctor* ctor, std::string_view filename, tr_error** error = nullptr);
bool tr_ctorSetMetainfoFromMagnetLink(tr_ctor* ctor, std::string_view magnet_link, tr_error** error = nullptr);
void tr_ctorSetLabels(tr_ctor* ctor, tr_quark const* labels, size_t n_labels);
void tr_ctorSetBandwidthPriority(tr_ctor* ctor, tr_priority_t priority);
tr_priority_t tr_ctorGetBandwidthPriority(tr_ctor const* ctor);
tr_torrent::labels_t const& tr_ctorGetLabels(tr_ctor const* ctor);

void tr_ctorSetVerifyDoneCallback(tr_ctor* ctor, tr_torrent::VerifyDoneCallback&& callback);
tr_torrent::VerifyDoneCallback tr_ctorStealVerifyDoneCallback(tr_ctor* ctor);

#define tr_logAddCriticalTor(tor, msg) tr_logAddCritical(msg, (tor)->name())
#define tr_logAddErrorTor(tor, msg) tr_logAddError(msg, (tor)->name())
#define tr_logAddWarnTor(tor, msg) tr_logAddWarn(msg, (tor)->name())
#define tr_logAddInfoTor(tor, msg) tr_logAddInfo(msg, (tor)->name())
#define tr_logAddDebugTor(tor, msg) tr_logAddDebug(msg, (tor)->name())
#define tr_logAddTraceTor(tor, msg) tr_logAddTrace(msg, (tor)->name())
