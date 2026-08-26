#include "ui/dialogs/crsselectiondialog.h"

#include "gis/spatialreference.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace HydroCouple::Composer
{
  namespace
  {
    //! Role holding the entry's authority code on the item.
    constexpr int kAuthCodeRole = Qt::UserRole + 1;
  }

  CrsSelectionDialog::CrsSelectionDialog(QWidget *parent) : QDialog(parent)
  {
    setObjectName(QStringLiteral("crsSelectionDialog"));
    setWindowTitle(tr("Coordinate Reference System"));
    setModal(true);

    buildForm();
    refreshList();
  }

  CrsSelectionDialog::~CrsSelectionDialog() = default;

  void CrsSelectionDialog::buildForm()
  {
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("crsSearchEdit"));
    m_searchEdit->setPlaceholderText(
      tr("Search by name, area or code — for example: utm 17n"));
    m_searchEdit->setClearButtonEnabled(true);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->setObjectName(QStringLiteral("crsKindCombo"));
    m_kindCombo->addItem(tr("All systems"),
                         QVariant::fromValue(static_cast<int>(CrsKind::Any)));
    m_kindCombo->addItem(
      tr("Projected"),
      QVariant::fromValue(static_cast<int>(CrsKind::Projected)));
    m_kindCombo->addItem(
      tr("Geographic"),
      QVariant::fromValue(static_cast<int>(CrsKind::Geographic)));

    m_list = new QTreeWidget(this);
    m_list->setObjectName(QStringLiteral("crsList"));
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Name"), tr("Code"), tr("Area of use")});
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_preview = new QPlainTextEdit(this);
    m_preview->setObjectName(QStringLiteral("crsWktPreview"));
    m_preview->setReadOnly(true);
    m_preview->setMaximumHeight(140);
    m_preview->setPlaceholderText(tr("Select a system to see its definition."));

    m_buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &) { refreshList(); });
    connect(m_kindCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { refreshList(); });

    connect(m_list, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *)
            {
              m_selectedAuthCode =
                current ? current->data(0, kAuthCodeRole).toString()
                        : QString();

              updatePreview();
            });

    // Double-clicking a row is the same as choosing it and pressing OK; every
    // list of things to pick from behaves that way.
    connect(m_list, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *, int) { accept(); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_kindCombo);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_preview);
    layout->addWidget(m_buttons);

    resize(760, 620);
  }

  CrsKind CrsSelectionDialog::selectedKind() const
  {
    return static_cast<CrsKind>(m_kindCombo->currentData().toInt());
  }

  void CrsSelectionDialog::refreshList()
  {
    const QVector<CrsEntry> matches =
      searchCrsCatalog(m_searchEdit->text(), selectedKind());

    // Read before the list is emptied. clear() moves the current item to
    // nothing, which runs the selection handler and blanks the very code
    // that is about to be looked for — so a search typed after choosing a
    // system would silently lose the choice.
    const QString wanted = m_selectedAuthCode;

    m_list->setUpdatesEnabled(false);
    m_list->clear();

    QTreeWidgetItem *reselect = nullptr;

    for (const CrsEntry &entry : matches)
    {
      auto *item = new QTreeWidgetItem(
        m_list, {entry.name, entry.authCode(), entry.areaName});

      item->setData(0, kAuthCodeRole, entry.authCode());

      // The selection is held by code rather than by row, so narrowing the
      // search does not silently choose a different system than the one the
      // user had already picked.
      if (!wanted.isEmpty() && entry.authCode() == wanted)
      {
        reselect = item;
      }
    }

    m_list->setUpdatesEnabled(true);

    if (reselect)
    {
      m_list->setCurrentItem(reselect);
      m_list->scrollToItem(reselect);
    }

    updatePreview();
  }

  void CrsSelectionDialog::updatePreview()
  {
    QPushButton *ok = m_buttons->button(QDialogButtonBox::Ok);

    if (m_selectedAuthCode.isEmpty())
    {
      m_preview->clear();
      ok->setEnabled(false);

      return;
    }

    QString message;
    const std::shared_ptr<SpatialReference> crs = selectedCrs(message);

    // Enabled on whether the definition actually resolves, not on whether a
    // row is highlighted: a code the database lists but PROJ cannot build is
    // better refused here than accepted and dropped later.
    ok->setEnabled(crs != nullptr);

    m_preview->setPlainText(
      crs ? QString::fromStdString(crs->srText()) : message);
  }

  void CrsSelectionDialog::setCurrentCrs(const SpatialReference *current)
  {
    if (!current)
    {
      return;
    }

    const QString authority = QString::fromStdString(current->authName());

    if (authority.isEmpty())
    {
      return;
    }

    m_selectedAuthCode =
      QStringLiteral("%1:%2").arg(authority).arg(current->authSRID());

    refreshList();
  }

  QString CrsSelectionDialog::selectedAuthCode() const
  {
    return m_selectedAuthCode;
  }

  std::shared_ptr<SpatialReference> CrsSelectionDialog::selectedCrs(
    QString &message) const
  {
    if (m_selectedAuthCode.isEmpty())
    {
      message = tr("No coordinate reference system is selected.");

      return nullptr;
    }

    const QString authority =
      m_selectedAuthCode.section(QLatin1Char(':'), 0, 0);
    const int code =
      m_selectedAuthCode.section(QLatin1Char(':'), 1, 1).toInt();

    return SpatialReference::fromAuthority(authority, code, message);
  }

} // namespace HydroCouple::Composer
