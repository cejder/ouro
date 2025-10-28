#include "loading.hpp"
#include "assert.hpp"
#include "render.hpp"
#include "string.hpp"

void loading_progress_forward(Loading *loading, C8 const *label) {
    loading->current++;
    d2d_loading_screen(loading, label);
}

void loading_finish(Loading *loading) {
    if (loading->current == loading->max) { return; }

    if (loading->current < loading->max) {
        _fixme_(TS("loading finished with %zu/%zu steps completed - mismatch with expected progress", loading->current, loading->max)->c);
        return;
    }

    _fixme_(TS("loading finished with %zu/%zu steps completed - exceeded predefined maximum", loading->current, loading->max)->c);
}

void loading_prepare(Loading *loading, SZ max) {
    loading->current = 0;
    loading->max     = max;
    d2d_loading_screen(loading, "Loading");
}

F32 loading_get_progress_perc(Loading *loading) {
    return (F32)loading->current / (F32)loading->max;
}
