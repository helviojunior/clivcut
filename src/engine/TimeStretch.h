// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace clv::engine {

/**
 * Mudança de velocidade sem alterar o tom (WSOLA: overlap-add de janelas
 * de ~32 ms, cada uma alinhada por correlação com a continuação natural da
 * anterior). Trabalha em estéreo intercalado, 48 kHz.
 *
 * A entrada é um fluxo contínuo de amostras indexado a partir de 0 desde o
 * último reset(); `Reader` preenche [índice, índice + quadros) e é chamado
 * sempre com índices crescentes. Com velocidade 1 a saída é idêntica à entrada.
 */
class TimeStretch {
public:
    using Reader = std::function<void(std::int64_t index, float *dst, int frames)>;

    TimeStretch();
    void reset();
    /** Gera `frames` quadros de saída consumindo a entrada a `speed` (0.05..20). */
    void process(float *out, int frames, double speed, const Reader &read);
    /** Posição atual de leitura, em quadros de entrada (para detectar deriva). */
    double inputPosition() const { return m_anaPos; }

private:
    static constexpr int kWindow = 1536;        ///< 32 ms
    static constexpr int kHop = kWindow / 2;    ///< saída avança meia janela
    static constexpr int kSearch = 384;         ///< ±8 ms de busca de alinhamento

    const float *input(std::int64_t start, int frames, const Reader &read);
    void synthesizeHop(double speed, const Reader &read);

    std::vector<float> m_window;     ///< Hann periódica
    std::vector<float> m_tail;       ///< segunda metade da janela anterior, já ponderada
    std::vector<float> m_ready;      ///< saída pronta ainda não entregue
    std::size_t m_readyPos = 0;
    double m_anaPos = 0;             ///< posição nominal da próxima janela (quadros de entrada)
    std::int64_t m_prevStart = -1;   ///< início real da janela anterior
    std::vector<float> m_in;         ///< cache da entrada
    std::int64_t m_inStart = 0;
};

} // namespace clv::engine
