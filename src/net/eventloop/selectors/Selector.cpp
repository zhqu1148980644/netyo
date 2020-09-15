#include "Selector.h"
#include "EpollSelector.h"


int Selector::EPOLL_WAIT_TIMEOUT = 10000;
int Selector::EVENTS_LIST_SIZE = 16;

Selector::Selector(Selector::EventHandler && handler)
    : handler(move(handler)) {}

unique_ptr<Selector> Selector::create_selector(Selector::EventHandler && handler) {
    return unique_ptr<Selector>{new EpollSelector(move(handler))};
}

