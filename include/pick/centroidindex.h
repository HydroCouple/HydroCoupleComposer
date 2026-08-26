/*!
 * \file   centroidindex.h
 * \author Caleb Buahin
 * \brief  CentroidIndex — "which of these things is at this point?"
 *
 * The question a drape asks of a terrain and the question a click asks of a
 * layer are the same question, and getting it right is the same short,
 * easily-wrong argument twice: a centroid says only roughly where a thing is,
 * so the nearest centroid's item frequently is not the item containing the
 * point. On a graded mesh — one coarse cell beside a column of fine ones —
 * the containing cell can be the tenth-nearest.
 *
 * The answer is to try the nearest first, because on any set of items that
 * are convex and comparable in size it is right and costs one descent of the
 * tree, and to widen to a radius search when it is not. Widening to the
 * largest item's own reach is *exact*, unlike guessing at a neighbour count:
 * an item whose centroid lies farther than that cannot contain the point.
 *
 * A KD-tree rather than a bin grid, because items that vary by three orders
 * of magnitude — which is every mesh generated to a channel — put either far
 * too many bins under the small ones or far too many items in one bin under
 * the large ones.
 *
 * The test is a template rather than a std::function: a drape asks this
 * question once per densified vertex, hundreds of thousands of times, and an
 * indirect call per candidate is the difference between inlining a couple of
 * comparisons and not.
 */

#ifndef HYDROCOUPLECOMPOSER_PICK_CENTROIDINDEX_H
#define HYDROCOUPLECOMPOSER_PICK_CENTROIDINDEX_H

#include <QPointF>

#include <nanoflann.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace HydroCouple::Composer
{

  /*!
   * \brief A searchable set of item positions.
   */
  class CentroidIndex
  {
    public:
      /*!
       * \brief Adds an item.
       * \param centroid Where the item is, roughly.
       * \param radius How far the item reaches from that centroid.
       * \param item The caller's identifier for it.
       */
      void add(const QPointF &centroid, double radius, int item)
      {
        m_cloud.x.push_back(centroid.x());
        m_cloud.y.push_back(centroid.y());
        m_cloud.item.push_back(item);
        m_maxRadius = std::max(m_maxRadius, radius);
      }

      /*!
       * \brief Builds the tree over everything added.
       *
       * Separate from add() because a KD-tree is built once over a whole set;
       * adding to a built one would silently search a stale tree.
       */
      void build()
      {
        m_tree.reset();

        if (!m_cloud.x.empty())
        {
          m_tree = std::make_unique<Tree>(2, m_cloud);
        }
      }

      /*!
       * \brief Discards everything.
       */
      void clear()
      {
        m_tree.reset();
        m_cloud = Cloud();
        m_maxRadius = 0.0;
      }

      /*!
       * \brief Whether there is anything to search.
       */
      [[nodiscard]] bool isValid() const { return m_tree != nullptr; }

      /*!
       * \brief How many items were added.
       */
      [[nodiscard]] int count() const { return int(m_cloud.x.size()); }

      /*!
       * \brief The farthest any item reaches from its own centroid.
       */
      [[nodiscard]] double maxRadius() const { return m_maxRadius; }

      /*!
       * \brief The nearest item to \a point, or -1.
       */
      [[nodiscard]] int nearest(const QPointF &point) const
      {
        if (!m_tree)
        {
          return -1;
        }

        const double query[2] = { point.x(), point.y() };

        size_t found = 0;
        double distance = 0.0;
        nanoflann::KNNResultSet<double> knn(1);
        knn.init(&found, &distance);

        return m_tree->findNeighbors(knn, query) ? m_cloud.item[found] : -1;
      }

      /*!
       * \brief The nearest item \a test accepts, or -1.
       *
       * \param point Where to search.
       * \param extraReach How far past the largest item's own reach a match
       *        may still lie — a click tolerance, say. Zero for containment.
       * \param test Called with an item; returns whether it is a match.
       */
      template<typename Test>
      [[nodiscard]] int findNearest(const QPointF &point, double extraReach,
                                    Test &&test) const
      {
        if (!m_tree)
        {
          return -1;
        }

        const double query[2] = { point.x(), point.y() };

        // The common case: the nearest centroid's item is the answer, on any
        // set that is convex and comparable in size.
        size_t found = 0;
        double distance = 0.0;
        nanoflann::KNNResultSet<double> knn(1);
        knn.init(&found, &distance);

        if (m_tree->findNeighbors(knn, query) && test(m_cloud.item[found]))
        {
          return m_cloud.item[found];
        }

        // It was not, so widen to every item that could possibly reach the
        // point. Sorted, so that the first match is the nearest one and a
        // click between two features takes the one it is closer to.
        const double reach = m_maxRadius + std::max(0.0, extraReach);

        std::vector<nanoflann::ResultItem<typename Tree::IndexType, double>>
          candidates;
        (void)m_tree->radiusSearch(query, reach * reach, candidates,
                                   nanoflann::SearchParameters(0.0f, true));

        for (const auto &candidate : candidates)
        {
          if (test(m_cloud.item[candidate.first]))
          {
            return m_cloud.item[candidate.first];
          }
        }

        return -1;
      }

    private:
      /*!
       * \brief Positions in the layout nanoflann reads.
       *
       * Structure of arrays rather than a vector of points: the tree reads
       * one coordinate at a time, and a vector of QPointF would make every
       * such read touch a cache line it uses half of.
       */
      struct Cloud
      {
          std::vector<double> x;
          std::vector<double> y;

          //! The caller's identifier for each position. Not the index into
          //! this cloud: callers routinely skip items, which leaves the two
          //! sequences different lengths, and answering with the wrong item
          //! looks like data that is slightly wrong rather than like a bug.
          std::vector<int> item;

          [[nodiscard]] size_t kdtree_get_point_count() const
          {
            return x.size();
          }

          [[nodiscard]] double kdtree_get_pt(size_t index,
                                             size_t dimension) const
          {
            return dimension == 0 ? x[index] : y[index];
          }

          template<typename BoundingBox>
          bool kdtree_get_bbox(BoundingBox &) const
          {
            return false;
          }
      };

      using Tree = nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<double, Cloud>, Cloud, 2>;

      Cloud m_cloud;
      std::unique_ptr<Tree> m_tree;
      double m_maxRadius = 0.0;
  };

} // namespace HydroCouple::Composer

#endif // HYDROCOUPLECOMPOSER_PICK_CENTROIDINDEX_H
