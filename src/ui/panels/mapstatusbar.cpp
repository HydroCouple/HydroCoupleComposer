#include "ui/panels/mapstatusbar.h"

#include "gis/spatialreference.h"
#include "map/mapcanvas.h"
#include "map/maptransform.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QRegularExpression>
#include <QToolButton>

namespace HydroCouple::Composer
{
  namespace
  {
    /*!
     * \brief The scales a chooser offers, as the N of 1:N.
     *
     * The ladder every GIS offers, roughly doubling: fine enough that the
     * next step is a recognisable change and coarse enough that the list
     * stays short.
     */
    const QVector<double> &scalePresets()
    {
      static const QVector<double> presets = {
        100.0,     250.0,     500.0,     1000.0,     2500.0,
        5000.0,    10000.0,   25000.0,   50000.0,    100000.0,
        250000.0,  500000.0,  1000000.0, 5000000.0,  25000000.0,
      };

      return presets;
    }

    QString formatScale(double denominator)
    {
      if (denominator <= 0.0)
      {
        return {};
      }

      // Grouped, because a denominator in the millions is unreadable
      // otherwise, and rounded, because a scale of 1:2500.37 is a precision
      // nobody asked for.
      return QStringLiteral("1:%1")
        .arg(QLocale::system().toString(qRound64(denominator)));
    }

    /*!
     * \brief Reads "1:2,500", "2500" or "1 : 2500" as a denominator.
     * \returns The denominator, or 0 when the text cannot be read.
     */
    double parseScale(const QString &text)
    {
      QString digits = text.section(QLatin1Char(':'), -1);

      // Everything that is not a digit or a decimal point goes: group
      // separators differ by locale, and a space is what people type.
      digits.remove(QRegularExpression(QStringLiteral("[^0-9.]")));

      bool ok = false;
      const double denominator = digits.toDouble(&ok);

      return ok && denominator > 0.0 ? denominator : 0.0;
    }

    QString describeCrs(const SpatialReference *crs)
    {
      if (!crs)
      {
        return QObject::tr("No CRS");
      }

      const QString authority = QString::fromStdString(crs->authName());

      // The authority code, not the name: it is what fits, what is
      // unambiguous, and what someone checking a projection is looking for.
      // The full name is on the tooltip.
      if (authority.isEmpty())
      {
        return crs->description();
      }

      return QStringLiteral("%1:%2").arg(authority).arg(crs->authSRID());
    }
  }

  MapStatusBar::MapStatusBar(QWidget *parent) : QWidget(parent)
  {
    setObjectName(QStringLiteral("mapStatusBar"));

    buildForm();
    setLive(false);
  }

  MapStatusBar::~MapStatusBar() = default;

  void MapStatusBar::buildForm()
  {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_coordinateLabel = new QLabel(this);
    m_coordinateLabel->setObjectName(QStringLiteral("coordinateLabel"));
    m_coordinateLabel->setAccessibleName(tr("Cursor coordinates"));
    m_coordinateLabel->setMinimumWidth(200);

    layout->addWidget(new QLabel(tr("Coordinates:"), this));
    layout->addWidget(m_coordinateLabel);

    m_scaleCombo = new QComboBox(this);
    m_scaleCombo->setObjectName(QStringLiteral("mapScaleCombo"));
    m_scaleCombo->setAccessibleName(tr("Map scale"));
    m_scaleCombo->setEditable(true);
    m_scaleCombo->setInsertPolicy(QComboBox::NoInsert);
    m_scaleCombo->setMinimumWidth(140);
    m_scaleCombo->setToolTip(tr("Map scale — pick a preset or type 1:N"));

    for (const double denominator : scalePresets())
    {
      m_scaleCombo->addItem(formatScale(denominator), denominator);
    }

    m_scaleCombo->setEditText(QString());

    // A preset click and a typed entry go through one slot, so the two
    // routes cannot come to different answers about what was asked for.
    connect(m_scaleCombo, &QComboBox::activated, this,
            [this](int index)
            {
              if (index >= 0)
              {
                applyScaleText(m_scaleCombo->itemText(index));
              }
            });
    connect(m_scaleCombo->lineEdit(), &QLineEdit::editingFinished, this,
            [this] { applyScaleText(m_scaleCombo->currentText()); });

    layout->addWidget(new QLabel(tr("Map scale:"), this));
    layout->addWidget(m_scaleCombo);

    m_crsButton = new QToolButton(this);
    m_crsButton->setObjectName(QStringLiteral("crsButton"));
    m_crsButton->setAccessibleName(tr("Coordinate reference system"));
    m_crsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    connect(m_crsButton, &QToolButton::clicked, this,
            [this] { Q_EMIT crsRequested(); });

    layout->addWidget(m_crsButton);

    refreshCrs();
  }

  void MapStatusBar::setCanvas(MapCanvas *canvas)
  {
    if (m_canvas == canvas)
    {
      return;
    }

    if (m_canvas)
    {
      m_canvas->disconnect(this);
    }

    m_canvas = canvas;

    if (m_canvas)
    {
      connect(m_canvas, &MapCanvas::cursorMoved, this,
              [this](const QPointF &world)
              {
                const SpatialReference *crs = m_canvas->crs();

                // Six decimals in degrees is about a tenth of a metre; in
                // metres it is a micrometre, which is a precision no map
                // has. The unit decides how much to print.
                const int decimals = crs && crs->isGeographic() ? 6 : 3;

                m_coordinateLabel->setText(QStringLiteral("%1, %2")
                                             .arg(world.x(), 0, 'f', decimals)
                                             .arg(world.y(), 0, 'f', decimals));
              });

      connect(m_canvas, &MapCanvas::scaleChanged, this,
              [this](double) { refreshScale(); });

      connect(m_canvas, &MapCanvas::crsChanged, this,
              [this] { refreshCrs(); });

      connect(m_canvas, &QObject::destroyed, this,
              [this] { setCanvas(nullptr); });
    }

    refreshScale();
    refreshCrs();
  }

  MapCanvas *MapStatusBar::canvas() const
  {
    return m_canvas;
  }

  void MapStatusBar::setLive(bool live)
  {
    m_scaleCombo->setEnabled(live && m_canvas);
    m_crsButton->setEnabled(live && m_canvas);

    // The coordinate readout used to keep its last 2D value on the 3D tab,
    // greyed-out-looking but never labelled stale -- a number that quietly
    // meant nothing about what was on screen. Off the map it is cleared,
    // and the camera readout takes the slot the moment the scene reports.
    if (!live)
    {
      m_coordinateLabel->setText(tr("3D view"));
    }
  }

  void MapStatusBar::showCameraReading(double azimuth, double elevation,
                                       double distance)
  {
    m_coordinateLabel->setText(
      tr("heading %1° · pitch %2° · %3 away")
        .arg(azimuth, 0, 'f', 0)
        .arg(elevation, 0, 'f', 0)
        .arg(distance, 0, 'f', 0));
  }

  QString MapStatusBar::scaleText() const
  {
    return m_scaleCombo->currentText();
  }

  void MapStatusBar::applyScaleText(const QString &text)
  {
    if (m_syncing || !m_canvas)
    {
      return;
    }

    const double denominator = parseScale(text);

    if (denominator <= 0.0)
    {
      // Unreadable: put back what the map is actually at, rather than
      // leaving the field showing something the view does not match.
      refreshScale();

      return;
    }

    m_canvas->setScaleDenominator(denominator);
  }

  void MapStatusBar::refreshScale()
  {
    if (!m_canvas)
    {
      m_scaleCombo->setEditText(QString());

      return;
    }

    // Guarded, because writing the text back would otherwise look like a
    // fresh edit and be applied to the canvas that just reported it.
    m_syncing = true;
    m_scaleCombo->setEditText(formatScale(m_canvas->scaleDenominator()));
    m_syncing = false;
  }

  void MapStatusBar::refreshCrs()
  {
    const SpatialReference *crs = m_canvas ? m_canvas->crs() : nullptr;

    m_crsButton->setText(describeCrs(crs));
    m_crsButton->setToolTip(
      crs ? tr("%1 — click to change the map's coordinate system")
              .arg(crs->description())
          : tr("Click to choose the map's coordinate system"));
  }

} // namespace HydroCouple::Composer
