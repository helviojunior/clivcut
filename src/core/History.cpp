// SPDX-License-Identifier: GPL-3.0-or-later
#include "History.h"

namespace clv {

void History::push(const Project &before, const QString &label)
{
    m_undo.push_back({before, label});
    if (m_undo.size() > m_limit)
        m_undo.pop_front();
    m_redo.clear();
    ++m_revision;
}

bool History::undo(Project &current)
{
    if (m_undo.empty())
        return false;
    Entry e = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back({current, e.label});
    current = std::move(e.state);
    --m_revision;
    return true;
}

bool History::redo(Project &current)
{
    if (m_redo.empty())
        return false;
    Entry e = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back({current, e.label});
    current = std::move(e.state);
    ++m_revision;
    return true;
}

void History::clear()
{
    m_undo.clear();
    m_redo.clear();
    m_revision = 0;
}

} // namespace clv
