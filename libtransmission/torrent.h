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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "transmission.h"

#include "announce-list.h"
#include "bandwidth.h"
#include "bitfield.h"
#include "block-info.h"
#include "completion.h"
#include "crypto-utils.h"
#include "file-piece-map.h"
#include "interned-string.h"
#include "log.h"
#include "session.h"
#include "torrent-metainfo.h"
#include "tr-macros.h"

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

enum tr_verify_state : uint8_t
{
    TR_VERIFY_NONE,
    TR_VERIFY_WAIT,
    TR_VERIFY_NOW
};

struct tr_incomplete_metadata;

/** @brief Torrent object */
struct tr_torrent final : public tr_completion::torrent_view
{
public:
    explicit tr_torrent(tr_torrent_metainfo&& tm)
        : metainfo_{ std::move(tm) }
        , completion{ this, &this->metainfo_.block_info() }
    {
    }

    void set_location(
        std::string_view location,
        bool move_from_old_path,
        double volatile* setme_progress,
        int volatile* setme_state);

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
        return this->announce_list().toString();
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
        return this->is_public() && this->session->allowsPEX();
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

    void set_local_error(std::string_view errmsg)
    {
        this->error = TR_STAT_LOCAL_ERROR;
        this->error_announce_url = TR_KEY_NONE;
        this->error_string = errmsg;
    }

    void set_download_dir(std::string_view path, bool is_new_torrent = false);

    void refresh_current_dir();

    void set_verify_state(tr_verify_state state);

    [[nodiscard]] constexpr auto verify_state() const noexcept
    {
        return verify_state_;
    }

    constexpr void set_verify_progress(float f) noexcept
    {
        verify_progress_ = f;
    }

    [[nodiscard]] constexpr std::optional<float> verify_progress() const noexcept
    {
        if (verify_state_ == TR_VERIFY_NOW)
        {
            return verify_progress_;
        }

        return {};
    }

    [[nodiscard]] constexpr auto const& id() const noexcept
    {
        return unique_id_;
    }

    constexpr void set_date_active(time_t t) noexcept
    {
        this->activityDate = t;

        if (this->anyDate < t)
        {
            this->anyDate = t;
        }
    }

    [[nodiscard]] constexpr auto activity() const noexcept
    {
        bool const is_seed = this->is_done();

        if (this->verify_state() == TR_VERIFY_NOW)
        {
            return TR_STATUS_CHECK;
        }

        if (this->verify_state() == TR_VERIFY_WAIT)
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

    void set_bandwidth_group(std::string_view group_name) noexcept;

    [[nodiscard]] constexpr auto get_priority() const noexcept
    {
        return bandwidth().get_priority();
    }

    [[nodiscard]] constexpr auto const& bandwidth_group() const noexcept
    {
        return bandwidth_group_;
    }

    [[nodiscard]] constexpr auto idle_limit_mode() const noexcept
    {
        return idle_limit_mode_;
    }

    [[nodiscard]] constexpr auto idle_limit_minutes() const noexcept
    {
        return idle_limit_minutes_;
    }

    [[nodiscard]] constexpr auto peer_limit() const noexcept
    {
        return max_connected_peers_;
    }

    constexpr void set_ratio_mode(tr_ratiolimit mode) noexcept
    {
        if (ratioLimitMode != mode)
        {
            ratioLimitMode = mode;
            set_dirty();
        }
    }

    constexpr void set_idle_limit(uint16_t idle_minutes) noexcept
    {
        if ((idle_limit_minutes_ != idle_minutes) && (idle_minutes > 0))
        {
            idle_limit_minutes_ = idle_minutes;
            set_dirty();
        }
    }

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

    tr_torrent_metainfo metainfo_;

    tr_bandwidth bandwidth_;

    tr_stat stats = {};

    // TODO(ckerr): make private once some of torrent.cc's `tr_torrentFoo()` methods are member functions
    tr_completion completion;

    // true iff the piece was verified more recently than any of the piece's
    // files' mtimes (file_mtimes_). If checked_pieces_.test(piece) is false,
    // it means that piece needs to be checked before its data is used.
    tr_bitfield checked_pieces_ = tr_bitfield{ 0 };

    tr_file_piece_map fpm_ = tr_file_piece_map{ metainfo_ };
    tr_file_priorities file_priorities_{ &fpm_ };
    tr_files_wanted files_wanted_{ &fpm_ };

    std::string error_string;

    using labels_t = std::vector<tr_quark>;
    labels_t labels;

    // when Transmission thinks the torrent's files were last changed
    std::vector<time_t> file_mtimes_;

    tr_sha1_digest_t obfuscated_hash = {};

    tr_session* session = nullptr;

    tr_torrent_announcer* torrent_announcer = nullptr;

    tr_swarm* swarm = nullptr;

    /* Used when the torrent has been created with a magnet link
     * and we're in the process of downloading the metainfo from
     * other peers */
    struct tr_incomplete_metadata* incompleteMetadata = nullptr;

    time_t lpdAnnounceAt = 0;

    time_t activityDate = 0;
    time_t addedDate = 0;
    time_t anyDate = 0;
    time_t doneDate = 0;
    time_t editDate = 0;
    time_t startDate = 0;

    time_t lastStatTime = 0;

    time_t seconds_downloading_before_current_start_ = 0;
    time_t seconds_seeding_before_current_start_ = 0;

    uint64_t downloadedCur = 0;
    uint64_t downloadedPrev = 0;
    uint64_t uploadedCur = 0;
    uint64_t uploadedPrev = 0;
    uint64_t corruptCur = 0;
    uint64_t corruptPrev = 0;

    uint64_t etaSpeedCalculatedAt = 0;

    tr_interned_string error_announce_url;

    // Where the files are when the torrent is complete.
    tr_interned_string download_dir_;

    // Where the files are when the torrent is incomplete.
    // a value of TR_KEY_NONE indicates the 'incomplete_dir' feature is unused
    tr_interned_string incomplete_dir_;

    // Where the files are now.
    // Will equal either download_dir or incomplete_dir
    tr_interned_string current_dir_;

    tr_stat_errtype error = TR_STAT_OK;

    tr_bytes_per_second_t etaSpeed_Bps = 0;

    size_t queuePosition = 0;

    tr_torrent_id_t unique_id_ = 0;

    tr_completeness completeness = TR_LEECH;

    float desiredRatio = 0.0F;
    tr_ratiolimit ratioLimitMode = TR_RATIOLIMIT_GLOBAL;

    tr_idlelimit idle_limit_mode_ = TR_IDLELIMIT_GLOBAL;

    uint16_t max_connected_peers_ = TR_DEFAULT_PEER_LIMIT_TORRENT;

    uint16_t idle_limit_minutes_ = 0;

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

    /* If the initiator of the connection receives a handshake in which the
     * peer_id does not match the expected peerid, then the initiator is
     * expected to drop the connection. Note that the initiator presumably
     * received the peer information from the tracker, which includes the
     * peer_id that was registered by the peer. The peer_id from the tracker
     * and in the handshake are expected to match.
     */
    tr_peer_id_t peer_id_ = tr_peerIdInit();

    tr_verify_state verify_state_ = TR_VERIFY_NONE;

    float verify_progress_ = -1;

    tr_announce_key_t announce_key_ = tr_rand_obj<tr_announce_key_t>();

    tr_interned_string bandwidth_group_;

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

void tr_torrentOnVerifyDone(tr_torrent* tor, bool aborted);

#define tr_logAddCriticalTor(tor, msg) tr_logAddCritical(msg, (tor)->name())
#define tr_logAddErrorTor(tor, msg) tr_logAddError(msg, (tor)->name())
#define tr_logAddWarnTor(tor, msg) tr_logAddWarn(msg, (tor)->name())
#define tr_logAddInfoTor(tor, msg) tr_logAddInfo(msg, (tor)->name())
#define tr_logAddDebugTor(tor, msg) tr_logAddDebug(msg, (tor)->name())
#define tr_logAddTraceTor(tor, msg) tr_logAddTrace(msg, (tor)->name())
