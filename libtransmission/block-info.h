// This file Copyright © 2021-2023 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include <cstdint> // uint32_t, uint64_t

#include "transmission.h"

struct tr_block_info
{
private:
    uint64_t total_size_ = 0;
    uint32_t piece_size_ = 0;
    tr_piece_index_t n_pieces_ = 0;

    tr_block_index_t n_blocks_ = 0;
    // should be same type as BlockSize
    uint32_t final_block_size_ = 0;
    // should be same type as piece_size
    uint32_t final_piece_size_ = 0;

public:
    static auto constexpr BlockSize = uint32_t{ 1024 * 16 };

    tr_block_info() noexcept = default;

    tr_block_info(uint64_t total_size_in, uint32_t piece_size_in) noexcept
    {
        init_sizes(total_size_in, piece_size_in);
    }

    void init_sizes(uint64_t total_size_in, uint32_t piece_size_in) noexcept;

    [[nodiscard]] constexpr auto block_count() const noexcept
    {
        return n_blocks_;
    }

    [[nodiscard]] constexpr auto block_size(tr_block_index_t block) const noexcept
    {
        return block + 1 == n_blocks_ ? final_block_size_ : BlockSize;
    }

    [[nodiscard]] constexpr auto piece_count() const noexcept
    {
        return n_pieces_;
    }

    [[nodiscard]] constexpr auto piece_size() const noexcept
    {
        return piece_size_;
    }

    [[nodiscard]] constexpr auto piece_size(tr_piece_index_t piece) const noexcept
    {
        return piece + 1 == n_pieces_ ? final_piece_size_ : piece_size();
    }

    [[nodiscard]] constexpr auto total_size() const noexcept
    {
        return total_size_;
    }

    struct Location
    {
        uint64_t byte = 0;

        tr_piece_index_t piece = 0;
        uint32_t piece_offset = 0;

        tr_block_index_t block = 0;
        uint32_t block_offset = 0;

        [[nodiscard]] constexpr bool operator==(Location const& that) const noexcept
        {
            return this->byte == that.byte;
        }

        [[nodiscard]] constexpr bool operator<(Location const& that) const noexcept
        {
            return this->byte < that.byte;
        }
    };

    // Location of the torrent's nth byte
    [[nodiscard]] constexpr auto byte_loc(uint64_t byte_idx) const noexcept
    {
        auto loc = Location{};

        if (is_initialized())
        {
            loc.byte = byte_idx;

            if (byte_idx == total_size()) // handle 0-byte files at the end of a torrent
            {
                loc.block = block_count() - 1;
                loc.piece = piece_count() - 1;
            }
            else
            {
                loc.block = byte_idx / BlockSize;
                loc.piece = byte_idx / piece_size();
            }

            loc.block_offset = static_cast<uint32_t>(loc.byte - (uint64_t{ loc.block } * BlockSize));
            loc.piece_offset = static_cast<uint32_t>(loc.byte - (uint64_t{ loc.piece } * piece_size()));
        }

        return loc;
    }

    // Location of the first byte in `block`.
    [[nodiscard]] constexpr auto block_loc(tr_block_index_t block) const noexcept
    {
        return byte_loc(uint64_t{ block } * BlockSize);
    }

    // Location of the first byte (+ optional offset and length) in `piece`
    [[nodiscard]] constexpr auto piece_loc(tr_piece_index_t piece, uint32_t offset = 0, uint32_t length = 0) const noexcept
    {
        return byte_loc(uint64_t{ piece } * piece_size() + offset + length);
    }

    [[nodiscard]] constexpr tr_block_span_t block_span_for_piece(tr_piece_index_t piece) const noexcept
    {
        if (!is_initialized())
        {
            return { 0U, 0U };
        }

        return { piece_loc(piece).block, piece_last_loc(piece).block + 1 };
    }

    [[nodiscard]] constexpr tr_byte_span_t byte_span_for_piece(tr_piece_index_t piece) const noexcept
    {
        if (!is_initialized())
        {
            return { 0U, 0U };
        }

        auto const offset = piece_loc(piece).byte;
        return { offset, offset + piece_size(piece) };
    }

private:
    // Location of the last byte in `piece`.
    [[nodiscard]] constexpr Location piece_last_loc(tr_piece_index_t piece) const noexcept
    {
        return byte_loc(static_cast<uint64_t>(piece) * piece_size() + piece_size(piece) - 1);
    }

    [[nodiscard]] constexpr bool is_initialized() const noexcept
    {
        return piece_size_ != 0;
    }
};
