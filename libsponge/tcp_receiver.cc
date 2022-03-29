#include "tcp_receiver.hh"

#include <optional>

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    bool ret = false;
    static size_t abs_seqno = 0;
    size_t len;
    if (seg.header().syn) {
        if (_syn_flag) {                // already get a SYN, refuse other SYN.
            return false;
        }
        _syn_flag = true;
        ret = true;
        _isn = seg.header().seqno.raw_value();
        abs_seqno = 1;
        _base = 1;
        len = seg.length_in_sequence_space() - 1;
        if (len == 0) {              // segment's content only have a SYN flag
            return true;
        }
    } else if (!_syn_flag) {            // before get a SYN, refuse any segment
        return false;
    } else {                            // not a SYN segment, compute it's abs_seqno
        abs_seqno = unwrap(WrappingInt32(seg.header().seqno.raw_value()),
                           WrappingInt32(_isn),
                           abs_seqno);
        len = seg.length_in_sequence_space();
    }

    if (seg.header().fin) {
        if (_fin_flag) {                    // already get a FIN, refuse other FIN
            return false;
        }
        _fin_flag = true;
    } else if (seg.length_in_sequence_space() == 0 && abs_seqno == _base) { // not FIN and not one size's SYN, check border
        return true;
    } else if (abs_seqno >= _base + window_size() || abs_seqno + len <= _base) {
        if (!ret) {
            return false;
        }
    }

    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
    _base = _reassembler.head_i() + 1;
    if (_reassembler.input_ended()) {       // FIN be count as one byte
        _base++;
    }
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_base > 0) {
        return WrappingInt32(wrap(_base, WrappingInt32(_isn)));
    } else {
        return std::nullopt;
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
