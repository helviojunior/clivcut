// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Model.h"

#include <QString>

#include <deque>

namespace clv {

/**
 * Undo/redo por snapshots. Um projeto típico tem poucos KB de estrutura,
 * então copiar o estado inteiro é simples, robusto e barato o bastante.
 */
class History {
public:
    explicit History(std::size_t limit = 200) : m_limit(limit) {}

    /** Registra o estado anterior a uma edição. */
    void push(const Project &before, const QString &label);
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    QString undoLabel() const { return m_undo.empty() ? QString() : m_undo.back().label; }
    QString redoLabel() const { return m_redo.empty() ? QString() : m_redo.back().label; }

    /** Troca `current` pelo estado anterior; retorna false se não houver. */
    bool undo(Project &current);
    bool redo(Project &current);
    void clear();

    /** Quantas edições desde o último save (para o indicador de "modificado"). */
    int revision() const { return m_revision; }

private:
    struct Entry {
        Project state;
        QString label;
    };
    std::deque<Entry> m_undo;
    std::deque<Entry> m_redo;
    std::size_t m_limit;
    int m_revision = 0;
};

} // namespace clv
