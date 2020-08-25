//
// Created by SwiftGan on 2020/8/21.
//

#pragma once

#include <cstddef>
#include <vector>
#include <base/marcos.h>

namespace Jit {

    struct CodeSet final {
        /// A single segment within a code set.
        struct Segment final {
            std::size_t offset = 0;

            VAddr addr = 0;

            u32 size = 0;
        };

        CodeSet(const std::string &modulePath, const std::array<Segment, 3> &segments,
                VAddr entrypoint) : module_path(modulePath), segments(segments),
                                    entrypoint(entrypoint) {}

        explicit CodeSet() = default;
        ~CodeSet() = default;

        CodeSet(const CodeSet&) = delete;
        CodeSet& operator=(const CodeSet&) = delete;

        CodeSet(CodeSet&&) = default;
        CodeSet& operator=(CodeSet&&) = default;

        Segment& CodeSegment() {
            return segments[0];
        }

        const Segment& CodeSegment() const {
            return segments[0];
        }

        Segment& RODataSegment() {
            return segments[1];
        }

        const Segment& RODataSegment() const {
            return segments[1];
        }

        Segment& DataSegment() {
            return segments[2];
        }

        const Segment& DataSegment() const {
            return segments[2];
        }

        std::string module_path;
        std::array<Segment, 3> segments;
        VAddr entrypoint = 0;
    };

}
