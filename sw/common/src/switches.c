#include "switches.h"

filter_id_t sw_decode(uint32_t sw, int *invalid)
{
    uint32_t b = sw & SW_FILTER_MASK;
    int bad = 0;
    filter_id_t f = FILTER_BYPASS;
    switch (b) {
    case 0u: f = FILTER_BYPASS; break;
    case 1u: f = FILTER_LOWPASS; break;
    case 2u: f = FILTER_BANDPASS; break;
    case 4u: f = FILTER_REVERB; break;
    default: bad = 1; break;              /* dos o mas switches: bypass */
    }
    if (invalid)
        *invalid = bad;
    return f;
}

void sw_init(sw_state_t *s, uint32_t sw)
{
    s->last_raw = s->stable_raw = sw & SW_FILTER_MASK;
    s->filter = sw_decode(s->stable_raw, &s->invalid);
}

int sw_update(sw_state_t *s, uint32_t sw)
{
    uint32_t b = sw & SW_FILTER_MASK;
    int same = (b == s->last_raw);
    s->last_raw = b;
    if (!same || b == s->stable_raw)
        return 0;
    int bad;
    filter_id_t f = sw_decode(b, &bad);
    int changed = (f != s->filter) || (bad != s->invalid);
    s->stable_raw = b;
    s->filter = f;
    s->invalid = bad;
    return changed;
}

const char *sw_filter_name(filter_id_t f)
{
    switch (f) {
    case FILTER_BYPASS:   return "BYPASS";
    case FILTER_LOWPASS:  return "PASA-BAJOS 1 kHz";
    case FILTER_BANDPASS: return "PASA-BANDA 1-4 kHz";
    case FILTER_REVERB:   return "REVERBERACION 50 ms";
    default:              return "?";
    }
}
