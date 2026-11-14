#include "layers/domainlayer.h"

#include "mesh/meshdomainmodel.h"
#include "render/layerstyle.h"

#include <QObject>

namespace HydroCouple::Composer
{
  namespace
  {
    //! The attribute each feature is indexed under, for labels and picking.
    const QString kIndexField = QStringLiteral("index");
  }

  DomainLayer::DomainLayer(const QString &name, MeshDomainModel *model,
                           DomainPart part)
    : FeatureLayer(name), m_model(model), m_part(part)
  {
  }

  DomainLayer::~DomainLayer() = default;

  QString DomainLayer::nameFor(DomainPart part)
  {
    switch (part)
    {
      case DomainPart::Boundary:
        return QObject::tr("Domain boundary");

      case DomainPart::Holes:
        return QObject::tr("Domain holes");

      case DomainPart::Breaklines:
        return QObject::tr("Domain breaklines");

      case DomainPart::ForcedPoints:
        return QObject::tr("Domain points");
    }

    return QObject::tr("Domain");
  }

  std::unique_ptr<DomainLayer> DomainLayer::create(MeshDomainModel *model,
                                                   DomainPart part)
  {
    std::unique_ptr<DomainLayer> layer(
      new DomainLayer(nameFor(part), model, part));

    QVector<AttributeField> fields;

    AttributeField index;
    index.name = kIndexField;
    index.displayName = QObject::tr("Index");
    fields.append(index);

    layer->setFields(fields);
    layer->applyDefaultStyle();

    if (model)
    {
      // The layer follows the model; the model knows nothing about layers.
      connect(model, &MeshDomainModel::domainChanged, layer.get(),
              [raw = layer.get()] { raw->rebuild(); });
      connect(model, &QObject::destroyed, layer.get(),
              [raw = layer.get()]
              {
                raw->m_model = nullptr;
                raw->rebuild();
              });
    }

    layer->rebuild();

    return layer;
  }

  DomainPart DomainLayer::part() const
  {
    return m_part;
  }

  MeshDomainModel *DomainLayer::model() const
  {
    return m_model;
  }

  void DomainLayer::applyDefaultStyle()
  {
    Symbol symbol = style()->symbol();

    switch (m_part)
    {
      case DomainPart::Boundary:
        // Outlined, not filled: the boundary is the edge of the ground, and
        // a filled one hides every layer the domain was drawn over.
        symbol.fill = QColor(0, 0, 0, 0);
        symbol.stroke = QColor(30, 90, 200);
        symbol.strokeWidth = 2.5;
        break;

      case DomainPart::Holes:
        // Filled, because a hole is an absence and the fill is what says so.
        symbol.fill = QColor(200, 60, 60, 70);
        symbol.stroke = QColor(200, 60, 60);
        symbol.strokeWidth = 1.5;
        break;

      case DomainPart::Breaklines:
        symbol.stroke = QColor(230, 140, 20);
        symbol.strokeWidth = 2.0;
        break;

      case DomainPart::ForcedPoints:
        symbol.fill = QColor(30, 90, 200);
        symbol.stroke = QColor(255, 255, 255);
        symbol.strokeWidth = 1.0;
        symbol.size = 6.0;
        break;
    }

    style()->setSymbol(symbol);
  }

  void DomainLayer::rebuild()
  {
    clearFeatures();

    if (m_model)
    {
      const MeshDomain &domain = m_model->domain();

      const auto addRing = [this](const QPolygonF &ring, GeometryKind kind,
                                  int index)
      {
        if (ring.isEmpty())
        {
          return;
        }

        VectorFeature feature;
        feature.kind = kind;
        feature.parts.append(ring);
        feature.attributes.append(QVariant(index));

        addFeature(std::move(feature));
      };

      switch (m_part)
      {
        case DomainPart::Boundary:
          addRing(domain.boundary, GeometryKind::Polygon, 0);
          break;

        case DomainPart::Holes:
          for (int index = 0; index < domain.holes.size(); ++index)
          {
            addRing(domain.holes.at(index), GeometryKind::Polygon, index);
          }
          break;

        case DomainPart::Breaklines:
          for (int index = 0; index < domain.constraintLines.size(); ++index)
          {
            addRing(domain.constraintLines.at(index), GeometryKind::Line,
                    index);
          }
          break;

        case DomainPart::ForcedPoints:
          for (int index = 0; index < domain.points.size(); ++index)
          {
            addRing(QPolygonF({domain.points.at(index)}),
                    GeometryKind::Point, index);
          }
          break;
      }
    }

    // finishLoading() would reset the symbol to a kind-appropriate default
    // and undo applyDefaultStyle(), so it is deliberately not called here.
    //
    // Both signals, because both are true: the picture changed, and so did
    // the ground it covers. A view that framed the layer would otherwise
    // keep framing the domain as it was drawn three edits ago.
    notifyExtentChanged();
    notifyAppearanceChanged();
  }

} // namespace HydroCouple::Composer
