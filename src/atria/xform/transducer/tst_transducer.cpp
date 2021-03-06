//
// Copyright (C) 2014, 2015 Ableton AG, Berlin. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#include <atria/xform/concepts.hpp>
#include <atria/xform/into.hpp>
#include <atria/xform/reducing/first_rf.hpp>
#include <atria/xform/transducer/filter.hpp>
#include <atria/xform/transducer/map.hpp>
#include <atria/xform/transducer/take.hpp>
#include <atria/xform/transducer/enumerate.hpp>
#include <atria/xform/transducer/transducer.hpp>
#include <atria/xform/transducer/traced.hpp>

#include <atria/testing/spies.hpp>
#include <atria/testing/gtest.hpp>

namespace atria {
namespace xform {

TEST(transducer, concept)
{
  meta::check<
    Transducer_spec(transducer<int>,
                    int, int)>();
  meta::check<
    Transducer_spec(transducer<meta::pack<int, float> >,
                    meta::pack<int, float>)>();
  meta::check<
    Transducer_spec(transducer<meta::pack<int, float>,
                               meta::pack<std::string, char*> >,
                    meta::pack<int, float>,
                    meta::pack<std::string, char*>)>();
}

TEST(transducer, type_erasure)
{
  auto v = std::vector<int> { 1, 2, 3, 4 };
  auto xform = transducer<int>{};

  {
    xform = map([] (int x) { return x + 2; });
    auto res = into(std::vector<int>{}, xform, v);
    EXPECT_EQ(res, (std::vector<int> { 3, 4, 5, 6 }));
  }

  {
    xform = filter([] (int x) { return x % 2; });
    auto res = into(std::vector<int>{}, xform, v);
    EXPECT_EQ(res, (std::vector<int> { 1, 3 }));
  }

  {
    xform = take(3);
    auto res = into(std::vector<int>{}, xform, v);
    EXPECT_EQ(res, (std::vector<int> { 1, 2, 3 }));
  }
}

TEST(transducer, variadic_type_erasure)
{
  auto xform = transducer<meta::pack<int, int>, int>{};
  xform = map([] (int a, int b) { return a + b; });
  auto res = into(std::vector<int>{}, xform,
                  std::vector<int> {1, 2, 3},
                  std::vector<int> {2, 3, 4});
  EXPECT_EQ(res, (std::vector<int> { 3, 5, 7 }));
}

TEST(transducer, variadic_output_type)
{
  auto xform = transducer<meta::pack<int, int> >{};
  xform = filter([] (int, int) { return true; });

  using tup = std::tuple<int, int>;
  auto res = into(std::vector<tup>{}, xform,
                  std::vector<int> {1, 2, 3},
                  std::vector<int> {2, 3, 4});

  EXPECT_EQ(res, (std::vector<tup> {
        tup(1, 2), tup(2, 3), tup(3, 4) }));
}

TEST(transducer, transforming_type_erasure)
{
  auto xform = transducer<int, std::string>{};
  xform = map([] (int a) { return std::to_string(a); });
  auto res = into(std::vector<std::string>{}, xform,
                  std::vector<int> {1, 2, 3});
  EXPECT_EQ(res, (std::vector<std::string> { "1", "2", "3" }));
}

TEST(transducer, type_erasure_and_composition)
{
  auto xform1 = transducer<std::string, int>{};
  auto xform2 = transducer<int, float>{};
  xform1 = map([] (std::string a) { return std::stoi(a); });
  xform2 = map([] (int a) { return float(a) / 2.0f; });

  auto xform3 = comp(xform1, xform2);
  auto res = into(std::vector<float>{}, xform3,
                  std::vector<std::string> {"1", "2", "3"});
  EXPECT_EQ(res, (std::vector<float> { 0.5f, 1.0f, 1.5f }));
}

TEST(transducer, type_erasure_and_composition_erased)
{
  auto xform1 = transducer<std::string, int>{};
  auto xform2 = transducer<int, float>{};
  xform1 = map([] (std::string a) { return std::stoi(a); });
  xform2 = map([] (int a) { return float(a) / 2.0f; });

  auto xform3 = transducer<std::string, float>{};
  xform3 = comp(xform1, xform2);
  auto res = into(std::vector<float>{}, xform3,
                  std::vector<std::string> {"1", "2", "3"});
  EXPECT_EQ(res, (std::vector<float> { 0.5f, 1.0f, 1.5f }));
}

TEST(transducer, type_erasure_and_composition_stateful_transducers)
{
  auto xform = transducer<int>{
    comp(transducer<int>{take(2)},
         transducer<int>{take(3)})};
  auto res = into(std::vector<int>{}, xform,
                  std::vector<int> { 1, 2, 3, 4, 5 });
  EXPECT_EQ(res, (std::vector<int> { 1, 2 }));
}

TEST(transducer, performs_minimal_moves)
{
  auto v = std::vector<int> { 1, 2, 3, 4, 5 };
  auto xform = transducer<int>{};

  {
    xform = map([] (int x) { return x + 2; });
    auto spy = reduce(xform(first_rf), testing::copy_spy<>{}, v);
    EXPECT_EQ(spy.copied.count(), 1);
  }

  {
    xform = filter([] (int x) { return x % 2; });
    auto spy = reduce(xform(first_rf), testing::copy_spy<>{}, v);
    EXPECT_EQ(spy.copied.count(), 1);
  }

  {
    xform = take(3);
    auto spy = reduce(xform(first_rf), testing::copy_spy<>{}, v);
    EXPECT_EQ(spy.copied.count(), 4);
  }
}

TEST(transducer, simple_transduction)
{
  transducer<int> xform = comp(
    filter([](int x) { return x % 2 == 0; }),
    map([](int x) { return x * 2; }));
  auto res = transduce(
    xform,
    std::plus<int>{},
    0,
    std::vector<int>{ 1, 2, 3, 4});
  EXPECT_EQ(res, 12);
}

TEST(transducer, generator)
{
  transducer<meta::pack<>, std::size_t> xform = comp(take(5), enumerate);
  auto res = transduce(
    xform,
    std::plus<std::size_t>{},
    std::size_t{});
  EXPECT_EQ(res, 10);
}

} // namespace xform
} // namespace atria
