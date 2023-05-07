// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "platformstyle.h"
#include "bip47/paymentcode.h"
#include "bip47/paymentchannel.h"

#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>

AddressBookPage::AddressBookPage(const PlatformStyle *platformStyle, Mode _mode, Tabs _tab, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddressBookPage),
    model(0),
    mode(_mode),
    tab(_tab)
{
    ui->setupUi(this);

    if (tab == SendingTab) {
        ui->addressType->addItem(tr("Spark"), Spark);
        ui->addressType->addItem(tr("Transparent"), Transparent);
        ui->addressType->addItem(tr("RAP"), RAP);
    } else {
        ui->addressType->addItem(tr(""), Transparent);
        ui->addressType->addItem(tr("Transparent"), Transparent);
        ui->addressType->hide();
    }

    if (!platformStyle->getImagesOnButtons()) {
        ui->newAddress->setIcon(QIcon());
        ui->copyAddress->setIcon(QIcon());
        ui->deleteAddress->setIcon(QIcon());
        ui->exportButton->setIcon(QIcon());
    } else {
        ui->newAddress->setIcon(platformStyle->SingleColorIcon(":/icons/add"));
        ui->copyAddress->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
        ui->deleteAddress->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
        ui->exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }

    switch(mode)
    {
    case ForSelection:
        switch(tab)
        {
        case SendingTab: setWindowTitle(tr("Choose the address to send coins to")); break;
        case ReceivingTab: setWindowTitle(tr("Choose the address to receive coins with")); break;
        }
        connect(ui->tableView, &QTableView::doubleClicked, this, &QDialog::accept);
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tableView->setFocus();
        ui->closeButton->setText(tr("C&hoose"));
        ui->exportButton->hide();
        break;
    case ForEditing:
        switch(tab)
        {
        case SendingTab: setWindowTitle(tr("Sending addresses")); break;
        case ReceivingTab: setWindowTitle(tr("Receiving addresses")); break;
        }
        break;
    }
    switch(tab)
    {
    case SendingTab:
        ui->labelExplanation->setText(tr("These are your Firo addresses for sending payments. Always check the amount and the receiving address before sending coins."));
        ui->deleteAddress->setVisible(true);
        break;
    case ReceivingTab:
        ui->labelExplanation->setText(tr("These are your Firo addresses for receiving payments. It is recommended to use a new receiving address for each transaction."));
        ui->deleteAddress->setVisible(false);
        break;
    }

    // Context menu actions
    copyAddressAction = new QAction(tr("&Copy Address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    deleteAction = new QAction(ui->deleteAddress->text(), this);

    // Build context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if(tab == SendingTab)
        contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();

    // Connect signals for context menu actions
    connect(copyAddressAction, &QAction::triggered, this, &AddressBookPage::on_copyAddress_clicked);
    connect(copyLabelAction, &QAction::triggered, this, &AddressBookPage::onCopyLabelAction);
    connect(editAction, &QAction::triggered, this, &AddressBookPage::onEditAction);
    connect(deleteAction, &QAction::triggered, this, &AddressBookPage::on_deleteAddress_clicked);

    connect(ui->tableView, &QWidget::customContextMenuRequested, this, &AddressBookPage::contextualMenu);

    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

AddressBookPage::~AddressBookPage()
{
    delete ui;
}

void AddressBookPage::setModel(AddressTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;
    proxyModel = new QSortFilterProxyModel(this);
    fproxyModel = new AddressBookFilterProxy(this);
    rproxyModel = new QSortFilterProxyModel(this);
    rfproxyModel = new AddressBookFilterProxy(this);
    internalSetMode();
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &AddressBookPage::selectionChanged);

    // Select row for newly created address
    connect(model, &AddressTableModel::rowsInserted, this, &AddressBookPage::selectNewAddress);

    selectionChanged();
    chooseAddressType(0);
    connect(ui->addressType, qOverload<int>(&QComboBox::activated), this, &AddressBookPage::chooseAddressType);
}

void AddressBookPage::internalSetMode()
{
    if (ui->addressType->currentText() == AddressTableModel::Transparent || ui->addressType->currentText() == AddressTableModel::Spark || ui->addressType->isHidden()) {
        proxyModel->setSourceModel(model);
        switch(tab)
        {
        case ReceivingTab:
            // Receive filter
            proxyModel->setFilterRole(AddressTableModel::TypeRole);
            proxyModel->setFilterFixedString(AddressTableModel::Receive);
            break;
        case SendingTab:
            // Send filter
            proxyModel->setFilterRole(AddressTableModel::TypeRole);
            proxyModel->setFilterFixedString(AddressTableModel::Send);
            break;
        }
        proxyModel->setDynamicSortFilter(true);
        proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);  
            
        fproxyModel->setSourceModel(proxyModel);
        fproxyModel->setDynamicSortFilter(true);
        fproxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        fproxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        ui->tableView->setModel(fproxyModel);
    } else {
        rproxyModel->setSourceModel(model->getPcodeAddressTableModel());
        rproxyModel->setDynamicSortFilter(true);
        rproxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        rproxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);  
            
        rfproxyModel->setSourceModel(rproxyModel);
        rfproxyModel->setDynamicSortFilter(true);
        rfproxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        rfproxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        ui->tableView->setModel(rfproxyModel);
    }

    // Set column widths
#if QT_VERSION < 0x050000
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::Address, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::AddressType, QHeaderView::Stretch);
#else
    ui->tableView->horizontalHeader()->setSectionResizeMode(AddressTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(AddressTableModel::Address, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(AddressTableModel::AddressType, QHeaderView::Stretch);
#endif
    ui->tableView->setTextElideMode(Qt::ElideMiddle);
}

void AddressBookPage::on_copyAddress_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}

void AddressBookPage::onCopyLabelAction()
{
     GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}

void AddressBookPage::onEditAction()
{
    QModelIndexList indexes;

    EditAddressDialog::Mode mode;
    AddressTableModel * pmodel;
    if (ui->addressType->currentText() == AddressTableModel::RAP) {
        mode = EditAddressDialog::EditPcode;
        pmodel = model->getPcodeAddressTableModel();
    } else if (ui->addressType->currentText() == AddressTableModel::Transparent) {
        mode = tab == SendingTab ? EditAddressDialog::EditSendingAddress : EditAddressDialog::EditReceivingAddress;
        pmodel = model;
    } else {
        mode = EditAddressDialog::EditSparkSendingAddress;
        pmodel = model;
    }

    if (!ui->tableView->selectionModel())
        return;
    indexes = ui->tableView->selectionModel()->selectedRows();
    if (!pmodel || indexes.isEmpty())
        return;

    EditAddressDialog dlg(mode, this);
    dlg.setModel(pmodel);
    QModelIndex origIndex1, origIndex2;
    if (ui->addressType->currentText() == AddressTableModel::RAP) {
        origIndex1 = rfproxyModel->mapToSource(indexes.at(0));
        origIndex2 = rproxyModel->mapToSource(origIndex1);
    } else {
        origIndex1 = fproxyModel->mapToSource(indexes.at(0));
        origIndex2 = proxyModel->mapToSource(origIndex1);
    }
    dlg.loadRow(origIndex2.row());
    dlg.exec();
}

void AddressBookPage::on_newAddress_clicked()
{
    if(!model)
        return;

    AddressTableModel *pmodel;
    EditAddressDialog::Mode mode;

    if (ui->addressType->currentText() == AddressTableModel::Spark) {
        pmodel = model;
        mode = EditAddressDialog::NewSparkSendingAddress;
    } else if (ui->addressType->currentText() == AddressTableModel::RAP) {
        pmodel = model->getPcodeAddressTableModel();
        mode = EditAddressDialog::NewPcode;
    } else {
        pmodel = model;
        mode = tab == SendingTab ? EditAddressDialog::NewSendingAddress : EditAddressDialog::NewReceivingAddress;
    }

    EditAddressDialog dlg(mode, this);
    dlg.setModel(pmodel);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

void AddressBookPage::on_deleteAddress_clicked()
{
    QTableView *table;
    table = ui->tableView;

    if(!table->selectionModel())
        return;

    QModelIndexList indexes = table->selectionModel()->selectedRows();

    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void AddressBookPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table;
    table = ui->tableView;

    // if(!table->selectionModel())
    //     return;

    // if(table->selectionModel()->hasSelection())
    if(true)
    {
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            ui->deleteAddress->setEnabled(true);
            ui->deleteAddress->setVisible(true);
            deleteAction->setEnabled(true);
            break;
        case ReceivingTab:
            // Deleting receiving addresses, however, is not allowed
            ui->deleteAddress->setEnabled(false);
            ui->deleteAddress->setVisible(false);
            deleteAction->setEnabled(false);
            break;
        }

        ui->copyAddress->setEnabled(true);
    }
    else
    {
        ui->deleteAddress->setEnabled(false);
        ui->copyAddress->setEnabled(false);
    }
}

void AddressBookPage::done(int retval)
{
    QTableView *table;
    table = ui->tableView;

    if(!table->selectionModel() || !table->model())
        return;

    // Figure out which address was selected, and return it
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    Q_FOREACH (const QModelIndex& index, indexes) {
        QVariant address = table->model()->data(index);
        returnValue = address.toString();
    }

    if(returnValue.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

void AddressBookPage::on_exportButton_clicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Export Address List"), QString(),
        tr("Comma separated file (*.csv)"), NULL);

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    QTableView *table;
    writer.setModel(proxyModel);
    if (ui->addressType->currentText() == AddressTableModel::Transparent) {
        writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
        writer.addColumn("Transparent Address", AddressTableModel::Address, Qt::EditRole);
        writer.addColumn("Address Type", AddressTableModel::AddressType, Qt::EditRole);
    } else if (ui->addressType->currentText() == AddressTableModel::RAP) {
        writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
        writer.addColumn("PaymentCode", AddressTableModel::Address, Qt::EditRole);
        writer.addColumn("Address Type", AddressTableModel::AddressType, Qt::EditRole);
    } else {
        writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
        writer.addColumn("Spark Address", AddressTableModel::Address, Qt::EditRole);
        writer.addColumn("Address Type", AddressTableModel::AddressType, Qt::EditRole);
    }

    if(!writer.write()) {
        QMessageBox::critical(this, tr("Exporting Failed"),
            tr("There was an error trying to save the address list to %1. Please try again.").arg(filename));
    }
}

void AddressBookPage::contextualMenu(const QPoint &point)
{
    QModelIndex index;
    index = ui->tableView->indexAt(point);

    if (ui->addressType->currentText() == "Spark") {
        copyAddressAction->setText(tr("&Copy Spark Address"));
    } else if (ui->addressType->currentText() == "RAP") {
        copyAddressAction->setText(tr("&Copy RAP address"));
    } else {
        copyAddressAction->setText(tr("&Copy Transparent Address"));
    }
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void AddressBookPage::selectNewAddress(const QModelIndex &parent, int begin, int /*end*/)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}

void AddressBookPage::chooseAddressType(int idx)
{
    internalSetMode();

    if (ui->addressType->currentText() == AddressTableModel::RAP) {
        if(!rproxyModel)
            return;
        rfproxyModel->setTypeFilter(
            ui->addressType->itemData(idx).toInt());
    } else {
        if(!proxyModel)
            return;
        fproxyModel->setTypeFilter(
            ui->addressType->itemData(idx).toInt());
    }
}

AddressBookFilterProxy::AddressBookFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

bool AddressBookFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 2, sourceParent);
    bool res0 = sourceModel()->data(index).toString().contains("spark");
    bool res1 = sourceModel()->data(index).toString().contains("transparent");
    bool res2 = sourceModel()->data(index).toString().contains("RAP");

    if(res0 && typeFilter == 0)
        return true;
    if(res1 && typeFilter == 1)
        return true;
    if(res2 && typeFilter == 2)
        return true;
    return false;
}

void AddressBookFilterProxy::setTypeFilter(quint32 modes)
{
    this->typeFilter = modes;
    invalidateFilter();
}