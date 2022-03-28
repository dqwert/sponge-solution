#include "stream_reassembler.hh"

#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    _buffer.resize(capacity);
}

long StreamReassembler::merge_seg(seg_t &to, const seg_t &from) {
    seg_t l, r;
    if (to.begin > from.begin) {
        l = from;
        r = to;
    } else {
        l = to;
        r = from;
    }
    if (l.begin + l.len < r.begin) {  // no intersection, couldn't merge
        return -1;
    } else if (l.begin + l.len >= r.begin + r.len) {  // l covers r completely
        to = l;
        return r.len;
    } else {  // l.b < r.b < l.e < r.e
        to.begin = l.begin;
        to.data = l.data + r.data.substr(l.begin + l.len - r.begin);
        to.len = to.data.length();
        return l.begin + l.len - r.begin;
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= _head_i + _capacity) {  // capacity over
        return;
    }

    // handle extra substring prefix
    seg_t elm;
    if (index + data.length() <= _head_i) {  // couldn't equal, because there have emtpy substring
        goto JUDGE_EOF;
    } else if (index < _head_i) {
        size_t offset = _head_i - index;
        elm.data.assign(data.begin() + offset, data.end());
        elm.begin = index + offset;
        elm.len = elm.data.length();
    } else {
        elm.begin = index;
        elm.len = data.length();
        elm.data = data;
    }
    _unassembled_byte += elm.len;

    // merge substring
    do {
        // merge next
        long merged_bytes = 0;
        auto iter = _segs.lower_bound(elm);
        while (iter != _segs.end() && (merged_bytes = merge_seg(elm, *iter)) >= 0) {
            _unassembled_byte -= merged_bytes;
            _segs.erase(iter);
            iter = _segs.lower_bound(elm);
        }
        // merge prev
        if (iter == _segs.begin()) {
            break;
        }
        iter--;
        while ((merged_bytes = merge_seg(elm, *iter)) >= 0) {
            _unassembled_byte -= merged_bytes;
            _segs.erase(iter);
            iter = _segs.lower_bound(elm);
            if (iter == _segs.begin()) {
                break;
            }
            iter--;
        }
    } while (false);
    _segs.insert(elm);

    // write to ByteStream
    if (!_segs.empty() && _segs.begin()->begin == _head_i) {
        const seg_t head_block = *_segs.begin();
        // modify _head_i and _unassembled_byte according to successful write to _output
        size_t write_bytes = _output.write(head_block.data);
        _head_i += write_bytes;
        _unassembled_byte -= write_bytes;
        _segs.erase(_segs.begin());
    }

JUDGE_EOF:
    if (eof) {
        _eof = true;
    }
    if (_eof && empty()) {
        _output.end_input();
    }
}
size_t StreamReassembler::unassembled_bytes() const { return _unassembled_byte; }

bool StreamReassembler::empty() const { return _unassembled_byte == 0; }
