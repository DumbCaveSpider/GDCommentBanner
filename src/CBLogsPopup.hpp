#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>
#include "CBLogsFilterPopup.hpp"

using namespace geode::prelude;

class CBLogsPopup : public Popup {
public:
    static CBLogsPopup* create();

private:
    bool init();
    void fetchLogs();
    void setFilter(CBLogsFilter const& filter);
    void updateFilterButton();
    void updatePagination();
    void onPrevPage();
    void onNextPage();

    static constexpr int kLogsPerPage = 10;

    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
    Button* m_filterBtn = nullptr;
    Button* m_prevBtn = nullptr;
    Button* m_nextBtn = nullptr;
    Label* m_pageLabel = nullptr;

    CBLogsFilter m_filter;
    int m_page = 0;
    int m_total = 0;
    int m_fetchId = 0;
};
