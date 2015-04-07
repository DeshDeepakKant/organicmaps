#pragma once

#include "coding/file_container.hpp"
#include "coding/mmap_reader.hpp"

#include "indexer/features_offsets_table.hpp"

#include "platform/platform.hpp"

#include "base/scope_guard.hpp"

#include "std/limits.hpp"
#include "std/string.hpp"
#include "std/unordered_map.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

#include "3party/succinct/rs_bit_vector.hpp"

#include "3party/succinct/elias_fano_compressed_list.hpp"

#include "defines.hpp"
namespace routing
{

typedef uint32_t TOsrmNodeId;
typedef vector<TOsrmNodeId> TNodesList;
extern TOsrmNodeId const INVALID_NODE_ID;

namespace OsrmMappingTypes {
#pragma pack (push, 1)
  struct FtSeg
  {
    uint32_t m_fid;
    uint16_t m_pointStart;
    uint16_t m_pointEnd;

    static constexpr uint32_t INVALID_FID = numeric_limits<uint32_t>::max();

    // No need to initialize something her (for vector<FtSeg>).
    FtSeg() {}
    FtSeg(uint32_t fid, uint32_t ps, uint32_t pe);

    explicit FtSeg(uint64_t x);
    uint64_t Store() const;

    bool Merge(FtSeg const & other);

    bool operator == (FtSeg const & other) const
    {
      return (other.m_fid == m_fid)
          && (other.m_pointEnd == m_pointEnd)
          && (other.m_pointStart == m_pointStart);
    }

    bool IsIntersect(FtSeg const & other) const;

    bool IsValid() const
    {
      return m_fid != INVALID_FID;
    }

    void Swap(FtSeg & other)
    {
      swap(m_fid, other.m_fid);
      swap(m_pointStart, other.m_pointStart);
      swap(m_pointEnd, other.m_pointEnd);
    }

    friend string DebugPrint(FtSeg const & seg);
  };

  struct SegOffset
  {
    TOsrmNodeId m_nodeId;
    uint32_t m_offset;

    SegOffset() : m_nodeId(0), m_offset(0) {}

    SegOffset(uint32_t nodeId, uint32_t offset)
      : m_nodeId(nodeId), m_offset(offset)
    {
    }

    friend string DebugPrint(SegOffset const & off);
  };

  struct FtSegLess
  {
    bool operator() (FtSeg const * a, FtSeg const * b) const
    {
      return a->Store() < b->Store();
    }
  };
#pragma pack (pop)
}
class OsrmFtSegMapping;

class OsrmFtSegBackwardIndex
{
  succinct::rs_bit_vector m_rankIndex;
  vector<TNodesList> m_nodeIds;
  unique_ptr<feature::FeaturesOffsetsTable> m_table;

  unique_ptr<MmapReader> m_mappedBits;

  void Save(string const & nodesFileName, string const & bitsFileName);

  bool Load(string const & nodesFileName, string const & bitsFileName);

public:
  void Construct(OsrmFtSegMapping & mapping, uint32_t const maxNodeId,
                 FilesMappingContainer & routingFile);

  TNodesList const &  GetNodeIdByFid(uint32_t const fid) const;

  void Clear();
};

class OsrmFtSegMapping
{
public:
  typedef set<OsrmMappingTypes::FtSeg*, OsrmMappingTypes::FtSegLess> FtSegSetT;

  void Clear();
  void Load(FilesMappingContainer & cont);

  void Map(FilesMappingContainer & cont);
  void Unmap();
  bool IsMapped() const;

  template <class ToDo> void ForEachFtSeg(TOsrmNodeId nodeId, ToDo toDo) const
  {
    pair<size_t, size_t> r = GetSegmentsRange(nodeId);
    while (r.first != r.second)
    {
      OsrmMappingTypes::FtSeg s(m_segments[r.first]);
      if (s.IsValid())
        toDo(s);
      ++r.first;
    }
  }

  typedef unordered_map<uint64_t, pair<TOsrmNodeId, TOsrmNodeId> > OsrmNodesT;
  void GetOsrmNodes(FtSegSetT & segments, OsrmNodesT & res,
                    atomic<bool> const & requestCancel) const;

  void GetSegmentByIndex(size_t idx, OsrmMappingTypes::FtSeg & seg) const;

  /// @name For debug purpose only.
  //@{
  void DumpSegmentsByFID(uint32_t fID) const;
  void DumpSegmentByNode(TOsrmNodeId nodeId) const;
  //@}

  /// @name For unit test purpose only.
  //@{
  /// @return STL-like range [s, e) of segments indexies for passed node.
  pair<size_t, size_t> GetSegmentsRange(uint32_t nodeId) const;
  /// @return Node id for segment's index.
  TOsrmNodeId GetNodeId(size_t segInd) const;

  size_t GetSegmentsCount() const { return m_segments.size(); }
  //@}

protected:
  typedef vector<OsrmMappingTypes::SegOffset> SegOffsetsT;
  SegOffsetsT m_offsets;

private:
  succinct::elias_fano_compressed_list m_segments;
  FilesMappingContainer::Handle m_handle;
  OsrmFtSegBackwardIndex m_backwardIndex;
};

class OsrmFtSegMappingBuilder : public OsrmFtSegMapping
{
public:
  OsrmFtSegMappingBuilder();

  typedef vector<OsrmMappingTypes::FtSeg> FtSegVectorT;

  void Append(TOsrmNodeId nodeId, FtSegVectorT const & data);
  void Save(FilesContainerW & cont) const;

private:
  vector<uint64_t> m_buffer;
  uint64_t m_lastOffset;
};

}
