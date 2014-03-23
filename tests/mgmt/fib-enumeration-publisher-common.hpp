/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (C) 2014 Named Data Networking Project
 * See COPYING for copyright and distribution information.
 */

#ifndef NFD_TESTS_MGMT_FIB_ENUMERATION_PUBLISHER_COMMON_HPP
#define NFD_TESTS_MGMT_FIB_ENUMERATION_PUBLISHER_COMMON_HPP

#include "mgmt/fib-enumeration-publisher.hpp"

#include "mgmt/app-face.hpp"
#include "mgmt/internal-face.hpp"

#include "tests/test-common.hpp"
#include "../face/dummy-face.hpp"

#include <ndn-cpp-dev/encoding/tlv.hpp>

namespace nfd {
namespace tests {

static inline uint64_t
readNonNegativeIntegerType(const Block& block,
                           uint32_t type)
{
  if (block.type() == type)
    {
      return readNonNegativeInteger(block);
    }
  std::stringstream error;
  error << "Expected type " << type << " got " << block.type();
  throw ndn::Tlv::Error(error.str());
}

static inline uint64_t
checkedReadNonNegativeIntegerType(Block::element_const_iterator& i,
                                  Block::element_const_iterator end,
                                  uint32_t type)
{
  if (i != end)
    {
      const Block& block = *i;
      ++i;
      return readNonNegativeIntegerType(block, type);
    }
  std::stringstream error;
  error << "Unexpected end of Block while attempting to read type #"
        << type;
  throw ndn::Tlv::Error(error.str());
}

class FibEnumerationPublisherFixture : public BaseFixture
{
public:

  FibEnumerationPublisherFixture()
    : m_nameTree(1024)
    , m_fib(m_nameTree)
    , m_face(make_shared<InternalFace>())
    , m_publisher(m_fib, m_face, "/localhost/nfd/FibEnumerationPublisherFixture")
    , m_finished(false)
  {

  }

  virtual
  ~FibEnumerationPublisherFixture()
  {

  }

  bool
  hasNextHopWithCost(const fib::NextHopList& nextHops,
                     FaceId faceId,
                     uint64_t cost)
  {
    for (fib::NextHopList::const_iterator i = nextHops.begin();
         i != nextHops.end();
         ++i)
      {
        if (i->getFace()->getId() == faceId && i->getCost() == cost)
          {
            return true;
          }
      }
    return false;
  }

  bool
  entryHasPrefix(const shared_ptr<fib::Entry> entry, const Name& prefix)
  {
    return entry->getPrefix() == prefix;
  }

  void
  validateFibEntry(const Block& entry)
  {
    entry.parse();

    Block::element_const_iterator i = entry.elements_begin();
    BOOST_REQUIRE(i != entry.elements_end());


    BOOST_REQUIRE(i->type() == ndn::Tlv::Name);
    Name prefix(*i);
    ++i;

    std::set<shared_ptr<fib::Entry> >::const_iterator referenceIter =
      std::find_if(m_referenceEntries.begin(), m_referenceEntries.end(),
                   boost::bind(&FibEnumerationPublisherFixture::entryHasPrefix,
                               this, _1, prefix));

    BOOST_REQUIRE(referenceIter != m_referenceEntries.end());

    const shared_ptr<fib::Entry>& reference = *referenceIter;
    BOOST_REQUIRE_EQUAL(prefix, reference->getPrefix());

    // 0 or more next hop records
    size_t nRecords = 0;
    const fib::NextHopList& referenceNextHops = reference->getNextHops();
    for (; i != entry.elements_end(); ++i)
      {
        const ndn::Block& nextHopRecord = *i;
        BOOST_REQUIRE(nextHopRecord.type() == ndn::tlv::nfd::NextHopRecord);
        nextHopRecord.parse();

        Block::element_const_iterator j = nextHopRecord.elements_begin();

        FaceId faceId =
          checkedReadNonNegativeIntegerType(j,
                                            entry.elements_end(),
                                            ndn::tlv::nfd::FaceId);

        uint64_t cost =
          checkedReadNonNegativeIntegerType(j,
                                            entry.elements_end(),
                                            ndn::tlv::nfd::Cost);

        BOOST_REQUIRE(hasNextHopWithCost(referenceNextHops, faceId, cost));

        BOOST_REQUIRE(j == nextHopRecord.elements_end());
        nRecords++;
      }
    BOOST_REQUIRE_EQUAL(nRecords, referenceNextHops.size());

    BOOST_REQUIRE(i == entry.elements_end());
    m_referenceEntries.erase(referenceIter);
  }

  void
  decodeFibEntryBlock(const Data& data)
  {
    Block payload = data.getContent();

    m_buffer.appendByteArray(payload.value(), payload.value_size());

    BOOST_CHECK_NO_THROW(data.getName()[-1].toSegment());
    if (data.getFinalBlockId() != data.getName()[-1])
      {
        return;
      }

    // wrap the FIB Entry blocks in a single Content TLV for easy parsing
    m_buffer.prependVarNumber(m_buffer.size());
    m_buffer.prependVarNumber(ndn::Tlv::Content);

    ndn::Block parser(m_buffer.buf(), m_buffer.size());
    parser.parse();

    BOOST_REQUIRE_EQUAL(parser.elements_size(), m_referenceEntries.size());

    for (Block::element_const_iterator i = parser.elements_begin();
         i != parser.elements_end();
         ++i)
      {
        if (i->type() != ndn::tlv::nfd::FibEntry)
          {
            BOOST_FAIL("expected fib entry, got type #" << i->type());
          }

        validateFibEntry(*i);
      }
    m_finished = true;
  }

protected:
  NameTree m_nameTree;
  Fib m_fib;
  shared_ptr<InternalFace> m_face;
  FibEnumerationPublisher m_publisher;
  ndn::EncodingBuffer m_buffer;
  std::set<shared_ptr<fib::Entry> > m_referenceEntries;

protected:
  bool m_finished;
};

} // namespace tests
} // namespace nfd

#endif // NFD_TESTS_MGMT_FIB_ENUMERATION_PUBLISHER_COMMON_HPP
