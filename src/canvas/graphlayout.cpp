#include "canvas/graphlayout.h"

#include <algorithm>
#include <numeric>

namespace HydroCouple::Composer
{

  namespace
  {
    //! What a component that names no installed library is drawn as.
    constexpr qreal kDefaultWidth = 160.0;
    constexpr qreal kDefaultHeight = 70.0;

    QSizeF sizeOf(const QHash<QString, QSizeF> &sizes, const QString &id)
    {
      const QSizeF size = sizes.value(id);

      return size.isValid() && !size.isEmpty()
               ? size
               : QSizeF(kDefaultWidth, kDefaultHeight);
    }
  } // namespace

  QHash<QString, QPointF> layoutComposition(
    const HydroCouple::SDK::IO::CompositionSpec &spec,
    const QHash<QString, QSizeF> &sizes, const LayoutOptions &options)
  {
    QStringList ids;

    for (const HydroCouple::SDK::IO::ComponentSpec &component :
         spec.components)
    {
      ids.append(QString::fromStdString(component.id));
    }

    if (ids.isEmpty())
    {
      return {};
    }

    // ── The graph, provider → consumer ───────────────────────────────────
    QHash<QString, QStringList> consumersOf;
    QHash<QString, QStringList> providersOf;

    for (const HydroCouple::SDK::IO::ConnectionSpec &connection :
         spec.connections)
    {
      const QString from = QString::fromStdString(connection.fromComponent);
      const QString to = QString::fromStdString(connection.toComponent);

      if (from == to || !ids.contains(from) || !ids.contains(to))
      {
        continue; // A self-loop tells the layout nothing.
      }

      consumersOf[from].append(to);
      providersOf[to].append(from);
    }

    // ── 1. Rank: longest path from something with no provider ────────────
    QHash<QString, int> rankOf;
    QStringList remaining = ids;
    int rank = 0;

    while (!remaining.isEmpty())
    {
      QStringList layer;

      for (const QString &id : remaining)
      {
        bool waiting = false;

        for (const QString &provider : providersOf.value(id))
        {
          if (remaining.contains(provider) && provider != id)
          {
            waiting = true;
            break;
          }
        }

        if (!waiting)
        {
          layer.append(id);
        }
      }

      // Everything left feeds something else left: a cycle. Its members
      // share this rank rather than the layout refusing to finish — a
      // feedback loop between two models is a normal coupling, not a
      // malformed document.
      if (layer.isEmpty())
      {
        layer = remaining;
      }

      for (const QString &id : layer)
      {
        rankOf.insert(id, rank);
        remaining.removeAll(id);
      }

      ++rank;
    }

    const int rankCount = rank;

    // ── 2. Order within each rank, by barycentre ─────────────────────────
    QList<QStringList> columns(rankCount);

    for (const QString &id : ids)
    {
      columns[rankOf.value(id)].append(id); // Document order to begin with.
    }

    QHash<QString, int> orderOf;

    for (int column = 0; column < rankCount; ++column)
    {
      for (int row = 0; row < columns[column].size(); ++row)
      {
        orderOf.insert(columns[column].at(row), row);
      }
    }

    // Two sweeps: enough to settle the common shapes, and cheap. Each
    // component slides towards the average row of what feeds it, so the
    // connections between two columns cross as little as they can.
    for (int sweep = 0; sweep < 2; ++sweep)
    {
      for (int column = 1; column < rankCount; ++column)
      {
        QStringList &members = columns[column];

        QHash<QString, double> barycentre;

        for (const QString &id : members)
        {
          const QStringList providers = providersOf.value(id);
          double total = 0.0;
          int counted = 0;

          for (const QString &provider : providers)
          {
            if (rankOf.value(provider) < column)
            {
              total += orderOf.value(provider);
              ++counted;
            }
          }

          // Nothing above it: stay where it is, rather than being dragged
          // to the top and pushing everything else down.
          barycentre.insert(id, counted > 0 ? total / counted
                                            : orderOf.value(id));
        }

        std::stable_sort(members.begin(), members.end(),
                         [&barycentre](const QString &lhs,
                                       const QString &rhs) {
                           return barycentre.value(lhs) < barycentre.value(rhs);
                         });

        for (int row = 0; row < members.size(); ++row)
        {
          orderOf.insert(members.at(row), row);
        }
      }
    }

    // ── 3. Place ─────────────────────────────────────────────────────────
    // Column width is the widest member, so a column of narrow boxes does
    // not leave a gap the size of the widest box in the document.
    QList<qreal> columnWidth(rankCount, kDefaultWidth);
    QList<qreal> columnHeight(rankCount, 0.0);

    for (int column = 0; column < rankCount; ++column)
    {
      qreal height = 0.0;

      for (const QString &id : columns[column])
      {
        const QSizeF size = sizeOf(sizes, id);
        columnWidth[column] = std::max(columnWidth[column], size.width());
        height += size.height() + options.rowGap;
      }

      columnHeight[column] = height - options.rowGap;
    }

    const qreal tallest =
      columnHeight.isEmpty()
        ? 0.0
        : *std::max_element(columnHeight.begin(), columnHeight.end());

    QHash<QString, QPointF> positions;
    qreal x = options.origin.x();

    for (int column = 0; column < rankCount; ++column)
    {
      // Centred against the tallest column, so a rank holding one component
      // sits beside the middle of the rank it feeds rather than above it.
      qreal y = options.origin.y() + (tallest - columnHeight[column]) * 0.5;

      for (const QString &id : columns[column])
      {
        const QSizeF size = sizeOf(sizes, id);

        positions.insert(id, QPointF(x, y));
        y += size.height() + options.rowGap;
      }

      x += columnWidth[column] + options.columnGap;
    }

    return positions;
  }

} // namespace HydroCouple::Composer
