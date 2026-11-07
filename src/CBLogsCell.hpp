#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

struct CBLogEntry {
    int id = 0;
    std::string actionType;
    std::string message;
    std::string createdAt;
};

class CBLogsCell : public CCNode {
public:
    static CBLogsCell* create(CBLogEntry const& entry, float width);
    static std::string titleForType(std::string const& actionType);

private:
    bool init(CBLogEntry const& entry, float width);
    CCNode* createIcon(std::string const& actionType);

    CBLogEntry m_entry;
};
