#pragma once

#include <DirectXMath.h>
#include <vector>

#define OT_SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))
#define OT_8EXPSUM(k) (((1 << (3 * (k))) - 1) / 7)
#define OT_NIL UINT32_MAX
#define OT_WITHIN_RANGE(p) (0.0f <= (p) && (p) < 1.0f)
#define OT_OUT_OF_RANGE(p) ((p) < 0.0f || 1.0f <= (p))

template <typename T> class Octree {
public:
  struct Cell {
    uint32_t XLocCode = 0;
    uint32_t YLocCode = 0;
    uint32_t ZLocCode = 0;
    uint32_t Level = 0;

    T data;
  };

public:
  Octree(uint32_t N_LEVELS);
  ~Octree() = default;

  uint32_t LocatePoint(const DirectX::XMFLOAT3 &p);
  uint32_t LocatePoint(const DirectX::XMFLOAT3 &p, uint32_t level);
  uint32_t LocateRegion(const DirectX::XMFLOAT3 &v_min,
                        const DirectX::XMFLOAT3 &v_max);
  uint32_t LocateRegion(const DirectX::XMFLOAT3 &v_min,
                        const DirectX::XMFLOAT3 &v_max, uint32_t level);
  uint32_t RayCastNext(uint32_t curr, const DirectX::XMFLOAT3 &p,
                       const DirectX::XMFLOAT3 &u);

  uint32_t Parent(uint32_t idx);
  uint32_t Child(uint32_t idx, uint32_t k);

  uint32_t GetLevels();
  uint32_t GetRootLevel();

  void CellToAABB(uint32_t idx, DirectX::XMFLOAT3 &v_min,
                  DirectX::XMFLOAT3 &v_max);
  void CellToAABB(const Cell &cell, DirectX::XMFLOAT3 &v_min,
                  DirectX::XMFLOAT3 &v_max);

  T &ReceiveData(uint32_t idx);

private:
  void EnCode();

  uint32_t LocateNeighbor(uint32_t idx, int x, int y, int z);
  uint32_t LocateCell(uint32_t xLocCode, uint32_t yLocCode, uint32_t zLocCode,
                      uint32_t level);

  float TimeToEscape(float v_m, float v_M, float p, float u);

public:
  const unsigned int m_Levels;
  const unsigned int m_RootLevel;
  const float m_MaxValue;
  const float m_CellSize;

  std::vector<Cell> m_Cells;
};

template <typename T>
Octree<T>::Octree(uint32_t N_LEVELS)
    : m_Levels(N_LEVELS), m_RootLevel(N_LEVELS - 1),
      m_MaxValue((float)(1 << (N_LEVELS - 1))), m_CellSize(1 / m_MaxValue) {
  m_Cells.resize(OT_8EXPSUM(N_LEVELS));

  EnCode();
}

template <typename T>
inline uint32_t Octree<T>::LocatePoint(const DirectX::XMFLOAT3 &p) {
  return LocatePoint(p, 0);
}

template <typename T>
inline uint32_t Octree<T>::LocatePoint(const DirectX::XMFLOAT3 &p,
                                       uint32_t level) {
  if (OT_OUT_OF_RANGE(p.x) || OT_OUT_OF_RANGE(p.y) || OT_OUT_OF_RANGE(p.z))
    return OT_NIL;
  uint32_t xLocCode = (uint32_t)(p.x * m_MaxValue);
  uint32_t yLocCode = (uint32_t)(p.y * m_MaxValue);
  uint32_t zLocCode = (uint32_t)(p.z * m_MaxValue);
  return LocateCell(xLocCode, yLocCode, zLocCode, level);
}

template <typename T>
inline uint32_t Octree<T>::LocateRegion(const DirectX::XMFLOAT3 &v_min,
                                        const DirectX::XMFLOAT3 &v_max) {
  return LocateRegion(v_min, v_max, 0);
}

template <typename T>
inline uint32_t Octree<T>::LocateRegion(const DirectX::XMFLOAT3 &v_min,
                                        const DirectX::XMFLOAT3 &v_max,
                                        uint32_t level) {
  if (OT_OUT_OF_RANGE(v_min.x) || OT_OUT_OF_RANGE(v_min.y) ||
      OT_OUT_OF_RANGE(v_min.z))
    return OT_NIL;
  if (OT_OUT_OF_RANGE(v_max.x) || OT_OUT_OF_RANGE(v_max.y) ||
      OT_OUT_OF_RANGE(v_max.z))
    return OT_NIL;

  uint32_t x0LocCode = (uint32_t)(v_min.x * m_MaxValue);
  uint32_t y0LocCode = (uint32_t)(v_min.y * m_MaxValue);
  uint32_t z0LocCode = (uint32_t)(v_min.z * m_MaxValue);
  uint32_t x1LocCode = (uint32_t)(v_max.x * m_MaxValue);
  uint32_t y1LocCode = (uint32_t)(v_max.y * m_MaxValue);
  uint32_t z1LocCode = (uint32_t)(v_max.z * m_MaxValue);

  uint32_t xDiff = x0LocCode ^ x1LocCode;
  uint32_t yDiff = y0LocCode ^ y1LocCode;
  uint32_t zDiff = z0LocCode ^ z1LocCode;

  int32_t xLevel = m_RootLevel;
  int32_t yLevel = m_RootLevel;
  int32_t zLevel = m_RootLevel;

  while (xLevel >= 0 && !(xDiff & (1 << xLevel)))
    xLevel--;
  while (yLevel > xLevel && !(yDiff & (1 << yLevel)))
    yLevel--;
  while (zLevel > yLevel && !(zDiff & (1 << zLevel)))
    zLevel--;
  zLevel++;

  return LocateCell(x0LocCode, y0LocCode, z0LocCode,
                    level > zLevel ? level : zLevel);
}

template <typename T>
inline uint32_t Octree<T>::RayCastNext(uint32_t curr,
                                       const DirectX::XMFLOAT3 &p,
                                       const DirectX::XMFLOAT3 &u) {
  if (curr >= m_Cells.size())
    return OT_NIL;

  DirectX::XMFLOAT3 v_min;
  DirectX::XMFLOAT3 v_max;
  CellToAABB(m_Cells[curr], v_min, v_max);

  float tx = TimeToEscape(v_min.x, v_max.x, p.x, u.x);
  float ty = TimeToEscape(v_min.y, v_max.y, p.y, u.y);
  float tz = TimeToEscape(v_min.z, v_max.z, p.z, u.z);

  int x = OT_SIGN(u.x);
  int y = OT_SIGN(u.y);
  int z = OT_SIGN(u.z);

  if (ty - tx > FLT_EPSILON) // tx < ty
  {
    if (tz - tx > FLT_EPSILON) // tx < tz && tx < ty
      return LocateNeighbor(curr, x, 0, 0);
    else if (tx - tz > FLT_EPSILON) // tz < tx && tx < ty
      return LocateNeighbor(curr, 0, 0, z);
    else // tx ~= tz < ty
      return LocateNeighbor(curr, x, 0, z);
  } else if (tx - ty > FLT_EPSILON) // ty < tx
  {
    if (tz - ty > FLT_EPSILON) // ty < tz && ty < tx
      return LocateNeighbor(curr, 0, y, 0);
    else if (ty - tz > FLT_EPSILON) // tz < ty && ty < tx
      return LocateNeighbor(curr, 0, 0, z);
    else // tz ~= ty < tx
      return LocateNeighbor(curr, 0, y, z);
  } else // tx ~= ty
  {
    if (tz - tx > FLT_EPSILON) // tx ~= ty < tz
      return LocateNeighbor(curr, x, y, 0);
    else if (tx - tz > FLT_EPSILON) // tz < tx ~= ty
      return LocateNeighbor(curr, 0, 0, z);
    else // tx ~= ty ~= tz
      return LocateNeighbor(curr, x, y, z);
  }
}

template <typename T> inline uint32_t Octree<T>::Parent(uint32_t idx) {
  uint32_t level = m_Cells[idx].Level;
  if (level == m_RootLevel)
    return OT_NIL;

  uint32_t xLocCode = m_Cells[idx].XLocCode;
  uint32_t yLocCode = m_Cells[idx].YLocCode;
  uint32_t zLocCode = m_Cells[idx].ZLocCode;
  return LocateCell(xLocCode, yLocCode, zLocCode, level + 1);
}

template <typename T>
inline uint32_t Octree<T>::Child(uint32_t idx, uint32_t k) {
  uint32_t level = m_Cells[idx].Level;
  if (level == 0)
    return OT_NIL;

  uint32_t xLocCode = m_Cells[idx].XLocCode;
  uint32_t yLocCode = m_Cells[idx].YLocCode;
  uint32_t zLocCode = m_Cells[idx].ZLocCode;

  uint32_t xChildBit = ((k & 0b100) >> 2) << (level - 1);
  uint32_t yChildBit = ((k & 0b010) >> 1) << (level - 1);
  uint32_t zChildBit = ((k & 0b001) >> 0) << (level - 1);
  return LocateCell(xLocCode | xChildBit, yLocCode | yChildBit,
                    zLocCode | zChildBit, level - 1);
}

template <typename T> inline uint32_t Octree<T>::GetLevels() {
  return m_Levels;
}

template <typename T> inline uint32_t Octree<T>::GetRootLevel() {
  return m_RootLevel;
}

template <typename T>
inline void Octree<T>::CellToAABB(uint32_t idx, DirectX::XMFLOAT3 &v_min,
                                  DirectX::XMFLOAT3 &v_max) {
  CellToAABB(m_Cells[idx], v_min, v_max);
}

template <typename T>
inline void Octree<T>::CellToAABB(const Cell &cell, DirectX::XMFLOAT3 &v_min,
                                  DirectX::XMFLOAT3 &v_max) {
  v_min.x = cell.XLocCode * m_CellSize;
  v_min.y = cell.YLocCode * m_CellSize;
  v_min.z = cell.ZLocCode * m_CellSize;
  v_max.x = v_min.x + (float)(1 << cell.Level) * m_CellSize;
  v_max.y = v_min.y + (float)(1 << cell.Level) * m_CellSize;
  v_max.z = v_min.z + (float)(1 << cell.Level) * m_CellSize;
}

template <typename T> inline T &Octree<T>::ReceiveData(uint32_t idx) {
  return m_Cells[idx].data;
}

template <typename T> inline void Octree<T>::EnCode() {
  uint32_t level = m_RootLevel;
  uint32_t idx = 0;
  do {
    uint32_t range = 1 << (m_RootLevel - level);
    for (uint32_t x = 0; x < range; x++) {
      for (uint32_t y = 0; y < range; y++) {
        for (uint32_t z = 0; z < range; z++) {
          m_Cells[idx].Level = level;
          m_Cells[idx].XLocCode = x << level;
          m_Cells[idx].YLocCode = y << level;
          m_Cells[idx].ZLocCode = z << level;
          idx++;
        }
      }
    }
  } while (level--);
}

template <typename T>
inline uint32_t Octree<T>::LocateNeighbor(uint32_t idx, int x, int y, int z) {
  uint32_t xLocCode = m_Cells[idx].XLocCode;
  uint32_t yLocCode = m_Cells[idx].YLocCode;
  uint32_t zLocCode = m_Cells[idx].ZLocCode;
  uint32_t level = m_Cells[idx].Level;
  uint32_t cellSize = 1 << level;
  uint32_t range = 1 << m_RootLevel;

  if (x == -1 && xLocCode == 0)
    return OT_NIL;
  if (x == 1 && (xLocCode + cellSize) >= range)
    return OT_NIL;
  if (y == -1 && yLocCode == 0)
    return OT_NIL;
  if (y == 1 && (yLocCode + cellSize) >= range)
    return OT_NIL;
  if (z == -1 && zLocCode == 0)
    return OT_NIL;
  if (z == 1 && (zLocCode + cellSize) >= range)
    return OT_NIL;

  uint32_t xNeighborLocCode = xLocCode + (x << level);
  uint32_t yNeighborLocCode = yLocCode + (y << level);
  uint32_t zNeighborLocCode = zLocCode + (z << level);

  return LocateCell(xNeighborLocCode, yNeighborLocCode, zNeighborLocCode,
                    level);
}

template <typename T>
inline uint32_t Octree<T>::LocateCell(uint32_t xLocCode, uint32_t yLocCode,
                                      uint32_t zLocCode, uint32_t level) {
  uint32_t offset = OT_8EXPSUM(m_RootLevel - level);
  uint32_t xBit = xLocCode >> level;
  uint32_t yBit = yLocCode >> level;
  uint32_t zBit = zLocCode >> level;
  uint32_t s = m_Levels - level - 1;
  uint32_t index = (xBit << (2 * s)) | (yBit << s) | zBit;

  return offset + index;
}

template <typename T>
inline float Octree<T>::TimeToEscape(float v_m, float v_M, float p, float u) {
  float v = abs(u);
  if (v < FLT_EPSILON)
    return FLT_MAX;
  float l = (u > 0) ? (v_M - p) : (p - v_m);
  float t = l / v;
  return t;
}
