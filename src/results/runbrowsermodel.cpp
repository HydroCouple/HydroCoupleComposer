#include "results/runbrowsermodel.h"

#include <QLocale>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Bits the level occupies in an index's internal id.
    constexpr int kLevelBits = 2;

    //! Bits the run index occupies above the level.
    constexpr int kRunBits = 20;

    QString describeShape(const SDK::IO::ResultEntry &entry)
    {
      if (entry.shape.empty())
      {
        return QObject::tr("scalar");
      }

      QStringList extents;

      for (const int64_t extent : entry.shape)
      {
        extents.append(QString::number(extent));
      }

      // The multiplication sign, not an "x": the shape is a product of
      // extents and reads as one.
      return extents.join(QStringLiteral(" × "));
    }

    QString describeMesh(const SDK::IO::ResultEntry &entry)
    {
      if (entry.mesh.empty())
      {
        return {};
      }

      const QString mesh = QString::fromStdString(entry.mesh);

      if (!entry.location.has_value())
      {
        return mesh;
      }

      // Where on the mesh matters as much as which mesh: the same variable
      // on faces and on nodes is drawn two entirely different ways.
      return QStringLiteral("%1 (%2)")
        .arg(mesh,
             QString::fromStdString(
               SDK::IO::meshLocationName(entry.location.value())));
    }

    QString describeTime(const SDK::IO::ResultEntry &entry)
    {
      if (!entry.time.has_value())
      {
        // Not an error: a static field — bathymetry, a mesh's areas — has no
        // time axis, and saying "none" beats printing a zero-length one.
        return QObject::tr("static");
      }

      const SDK::IO::ResultTimeAxis &axis = entry.time.value();

      return QObject::tr("%1 steps, %2 → %3 %4")
        .arg(QString::number(axis.count),
             QLocale::system().toString(axis.start, 'g', 6),
             QLocale::system().toString(axis.end, 'g', 6),
             QString::fromStdString(axis.units));
    }
  }

  RunBrowserModel::RunBrowserModel(QObject *parent)
    : QAbstractItemModel(parent)
  {
  }

  RunBrowserModel::~RunBrowserModel() = default;

  quintptr RunBrowserModel::encode(const Location &location)
  {
    return static_cast<quintptr>(location.level)
           | (static_cast<quintptr>(location.run) << kLevelBits)
           | (static_cast<quintptr>(location.component)
              << (kLevelBits + kRunBits));
  }

  RunBrowserModel::Location RunBrowserModel::decode(quintptr id)
  {
    Location location;

    location.level = static_cast<Level>(id & ((1u << kLevelBits) - 1));
    location.run = int((id >> kLevelBits) & ((1u << kRunBits) - 1));
    location.component = int(id >> (kLevelBits + kRunBits));

    return location;
  }

  RunBrowserModel::Location RunBrowserModel::locate(
    const QModelIndex &index) const
  {
    Location location = decode(index.internalId());

    // The row is the last coordinate, whichever level this is: it is already
    // in the index, so carrying it in the id as well would be two places to
    // keep in step.
    switch (location.level)
    {
      case Level::Run:
        location.run = index.row();
        break;

      case Level::Component:
        location.component = index.row();
        break;

      case Level::Item:
        location.item = index.row();
        break;
    }

    return location;
  }

  RunSession *RunBrowserModel::addRun(const QString &manifestPath,
                                      QString &message)
  {
    std::unique_ptr<RunSession> session =
      RunSession::open(manifestPath, message);

    if (!session)
    {
      return nullptr;
    }

    const int row = int(m_runs.size());

    beginInsertRows(QModelIndex(), row, row);
    m_runs.push_back(std::move(session));
    endInsertRows();

    return m_runs.back().get();
  }

  void RunBrowserModel::removeRun(int row)
  {
    if (row < 0 || row >= int(m_runs.size()))
    {
      return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_runs.erase(m_runs.begin() + row);
    endRemoveRows();
  }

  int RunBrowserModel::runCount() const
  {
    return int(m_runs.size());
  }

  RunSession *RunBrowserModel::run(int row) const
  {
    if (row < 0 || row >= int(m_runs.size()))
    {
      return nullptr;
    }

    return m_runs[size_t(row)].get();
  }

  QVector<int> RunBrowserModel::runsCarrying(const QString &componentId,
                                             const QString &itemId) const
  {
    QVector<int> rows;

    const std::string component = componentId.toStdString();
    const std::string item = itemId.toStdString();

    for (int row = 0; row < runCount(); ++row)
    {
      const RunSession *session = run(row);

      if (session && session->manifest().entry(component, item))
      {
        rows.append(row);
      }
    }

    return rows;
  }

  RunSession *RunBrowserModel::runFor(const QModelIndex &index) const
  {
    if (!index.isValid())
    {
      return nullptr;
    }

    return run(locate(index).run);
  }

  QModelIndex RunBrowserModel::index(int row, int column,
                                     const QModelIndex &parent) const
  {
    if (row < 0 || column < 0 || column >= ColumnCount
        || row >= rowCount(parent))
    {
      return {};
    }

    if (!parent.isValid())
    {
      return createIndex(row, column, encode({Level::Run, row, 0, 0}));
    }

    const Location above = locate(parent);

    switch (above.level)
    {
      case Level::Run:
        return createIndex(
          row, column, encode({Level::Component, above.run, row, 0}));

      case Level::Component:
        return createIndex(row, column,
                           encode({Level::Item, above.run, above.component,
                                   row}));

      case Level::Item:
        // A recorded value has no parts. Descending into one would be the
        // view asking to draw a tree of numbers.
        return {};
    }

    return {};
  }

  QModelIndex RunBrowserModel::parent(const QModelIndex &child) const
  {
    if (!child.isValid())
    {
      return {};
    }

    const Location location = locate(child);

    switch (location.level)
    {
      case Level::Run:
        return {};

      case Level::Component:
        return createIndex(location.run, 0,
                           encode({Level::Run, location.run, 0, 0}));

      case Level::Item:
        return createIndex(
          location.component, 0,
          encode({Level::Component, location.run, location.component, 0}));
    }

    return {};
  }

  int RunBrowserModel::rowCount(const QModelIndex &parent) const
  {
    if (!parent.isValid())
    {
      return int(m_runs.size());
    }

    if (parent.column() > 0)
    {
      // Only the first column carries children, as every tree view expects;
      // answering otherwise draws the tree once per column.
      return 0;
    }

    const Location location = locate(parent);
    RunSession *session = run(location.run);

    if (!session)
    {
      return 0;
    }

    switch (location.level)
    {
      case Level::Run:
        return int(session->componentIds().size());

      case Level::Component:
      {
        const QStringList ids = session->componentIds();

        if (location.component < 0 || location.component >= ids.size())
        {
          return 0;
        }

        return int(session->entriesFor(ids.at(location.component)).size());
      }

      case Level::Item:
        return 0;
    }

    return 0;
  }

  int RunBrowserModel::columnCount(const QModelIndex &) const
  {
    return ColumnCount;
  }

  QVariant RunBrowserModel::data(const QModelIndex &index, int role) const
  {
    if (!index.isValid())
    {
      return {};
    }

    const Location location = locate(index);
    RunSession *session = run(location.run);

    if (!session)
    {
      return {};
    }

    const QStringList components = session->componentIds();

    const QString componentId =
      location.level != Level::Run && location.component >= 0
          && location.component < components.size()
        ? components.at(location.component)
        : QString();

    const SDK::IO::ResultEntry *entry = nullptr;

    if (location.level == Level::Item && !componentId.isEmpty())
    {
      const QVector<const SDK::IO::ResultEntry *> entries =
        session->entriesFor(componentId);

      if (location.item >= 0 && location.item < entries.size())
      {
        entry = entries.at(location.item);
      }
    }

    if (role == ComponentIdRole)
    {
      return componentId;
    }

    if (role == ItemIdRole)
    {
      return entry ? QString::fromStdString(entry->itemId) : QString();
    }

    if (role == Qt::ToolTipRole && entry)
    {
      // The artifact is what a reader needs when a row will not open, and
      // the description is what the recording component said it was.
      return QStringLiteral("%1\n%2 (%3)")
        .arg(entry->description.empty()
               ? QString::fromStdString(entry->itemId)
               : QString::fromStdString(entry->description),
             QString::fromStdString(entry->artifact),
             QString::fromStdString(entry->format));
    }

    if (role != Qt::DisplayRole)
    {
      return {};
    }

    const SDK::IO::RunManifest &manifest = session->manifest();

    switch (location.level)
    {
      case Level::Run:
        switch (index.column())
        {
          case NameColumn:
            return session->title();

          case KindColumn:
            return QString::fromStdString(
              SDK::IO::runStatusName(manifest.status));

          case TimeColumn:
            return QString::fromStdString(manifest.started);

          default:
            return {};
        }

      case Level::Component:
        switch (index.column())
        {
          case NameColumn:
            return componentId;

          case KindColumn:
            return tr("component");

          case ShapeColumn:
            return tr("%n recorded item(s)", nullptr,
                      int(session->entriesFor(componentId).size()));

          default:
            return {};
        }

      case Level::Item:
        if (!entry)
        {
          return {};
        }

        switch (index.column())
        {
          case NameColumn:
            return QString::fromStdString(entry->itemId);

          case KindColumn:
            return QString::fromStdString(SDK::IO::dataKindName(entry->kind));

          case ShapeColumn:
            return describeShape(*entry);

          case UnitsColumn:
            return QString::fromStdString(entry->units);

          case MeshColumn:
            return describeMesh(*entry);

          case TimeColumn:
            return describeTime(*entry);

          default:
            return {};
        }
    }

    return {};
  }

  QVariant RunBrowserModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const
  {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
      return {};
    }

    switch (section)
    {
      case NameColumn:
        return tr("Name");

      case KindColumn:
        return tr("Kind");

      case ShapeColumn:
        return tr("Shape");

      case UnitsColumn:
        return tr("Units");

      case MeshColumn:
        return tr("Mesh");

      case TimeColumn:
        return tr("Time");

      default:
        return {};
    }
  }

} // namespace HydroCouple::Composer
