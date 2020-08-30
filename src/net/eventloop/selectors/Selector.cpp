#include "Selector.h"
#include "EpollSelector.h"


Selector::Selector(Selector::EventHandler && handler)
    : handler(move(handler)) {}

unique_ptr<Selector> Selector::create_selector(Selector::EventHandler && handler) {
    return unique_ptr<Selector>{new EpollSelector(move(handler))};
}

