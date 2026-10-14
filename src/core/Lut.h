// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QString>

#include <array>
#include <memory>
#include <vector>

/**
 * LUTs 3D de cor: leitura de arquivos .cube (Adobe/Resolve) e os filtros
 * embutidos do clivcut, que são gerados por fórmulas (nenhum LUT de
 * terceiros é distribuído).
 */
namespace clv {

struct Lut3D {
    int size = 0;                ///< N: a tabela tem N³ entradas
    std::vector<float> rgb;      ///< N³×3, R varia mais rápido (ordem do .cube)
    std::array<float, 3> domainMin{0, 0, 0};
    std::array<float, 3> domainMax{1, 1, 1};
    QString title;

    bool isValid() const { return size >= 2 && rgb.size() == static_cast<size_t>(size) * size * size * 3; }
    /** Consulta com interpolação trilinear (entrada e saída 0..1). */
    std::array<float, 3> sample(float r, float g, float b) const;
};

/** Lê um .cube (3D; LUTs 1D são convertidos para 3D). Em erro retorna inválido e preenche `error`. */
Lut3D parseCube(const QByteArray &data, QString *error = nullptr);
Lut3D loadCube(const QString &path, QString *error = nullptr);
QByteArray toCube(const Lut3D &lut);

struct BuiltinFilter {
    QString id;
    QString name;     ///< rótulo da interface (pt-BR)
    QString category; ///< "color", "cinema", "retro", "bw"
};

const std::vector<BuiltinFilter> &builtinFilters();
const BuiltinFilter *findBuiltinFilter(const QString &id);

/** Gera o LUT de um filtro embutido (N = 33). Inválido se o id não existir. */
Lut3D builtinFilterLut(const QString &id, int size = 33);

/**
 * LUT por referência: id de filtro embutido ou caminho de .cube, com cache
 * (seguro entre threads). Retorna nulo se não puder carregar.
 */
std::shared_ptr<const Lut3D> lutForKey(const QString &key);

/** Aplica um LUT a uma imagem na CPU (miniaturas, testes). */
QImage applyLut(const QImage &img, const Lut3D &lut, float intensity = 1.0f);

} // namespace clv
