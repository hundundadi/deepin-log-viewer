#include "displaycontent.h"

#include <sys/utsname.h>
#include <DApplication>
#include <DApplicationHelper>
#include <DFileDialog>
#include <DHorizontalLine>
#include <DScrollBar>
#include <DStandardItem>
#include <QAbstractItemView>
#include <QDateTime>
#include <QDebug>
#include <QHeaderView>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QThread>
#include <QVBoxLayout>
#include "logexportwidget.h"
#include "logfileparser.h"

DWIDGET_USE_NAMESPACE

#define SINGLE_LOAD 500

DisplayContent::DisplayContent(QWidget *parent)
    : DWidget(parent)

{
    initUI();
    initMap();
    initConnections();
}

DisplayContent::~DisplayContent()
{
    if (m_treeView) {
        delete m_treeView;
        m_treeView = nullptr;
    }
    if (m_pModel) {
        delete m_pModel;
        m_pModel = nullptr;
    }
}

void DisplayContent::initUI()
{
    // init pointer
    m_dateTime = new DLabel(this);
    m_userName = new DLabel(this);
    m_pid = new DLabel(this);
    m_status = new DLabel(this);
    m_level = new LogIconButton(this);
    m_textBrowser = new DTextBrowser(this);

    QVBoxLayout *vLayout = new QVBoxLayout;

    // set table for display log data
    initTableView();

    // display single table row's detail info
    QVBoxLayout *vLy = new QVBoxLayout;
    QHBoxLayout *hLy1 = new QHBoxLayout;
    m_daemonName = new DLabel(this);
    QFont font;
    font.setBold(true);
    m_daemonName->setFont(font);
    hLy1->addWidget(m_daemonName);
    hLy1->addStretch(1);
    hLy1->addWidget(m_dateTime);

    QHBoxLayout *hLy2 = new QHBoxLayout;

    QHBoxLayout *hLy21 = new QHBoxLayout;
    m_userLabel = new DLabel(DApplication::translate("Label", "User:"), this);
    hLy21->addWidget(m_userLabel);
    hLy21->addWidget(m_userName, 1);

    QHBoxLayout *hLy22 = new QHBoxLayout;
    m_pidLabel = new DLabel(DApplication::translate("Label", "PID:"), this);
    hLy22->addWidget(m_pidLabel);
    hLy22->addWidget(m_pid, 1);

    QHBoxLayout *hLy23 = new QHBoxLayout;
    m_statusLabel = new DLabel(DApplication::translate("Label", "Status:"), this);
    hLy23->addWidget(m_statusLabel);
    hLy23->addWidget(m_status, 1);

    hLy2->addLayout(hLy21, 1);
    hLy2->addLayout(hLy22, 1);
    hLy2->addLayout(hLy23, 1);
    hLy2->addStretch(1);
    hLy2->addWidget(m_level);

    vLy->addLayout(hLy1);
    vLy->addLayout(hLy2);
    DHorizontalLine *hline = new DHorizontalLine;
    hline->setLineWidth(3);
    vLy->addWidget(hline);
    vLy->addWidget(m_textBrowser, 2);
    vLy->setContentsMargins(0, 0, 0, 0);

    // layout for widgets
    vLayout->addWidget(m_treeView, 5);

    noResultLabel = new DLabel;
    noResultLabel->setText(DApplication::translate("SearchBar", "No search results"));
    QFont labelFont;
    labelFont.setPixelSize(20);
    noResultLabel->setFont(labelFont);
    noResultLabel->setAlignment(Qt::AlignCenter);
    vLayout->addWidget(noResultLabel, 5);
    noResultLabel->hide();

    m_spinnerWgt = new LogSpinnerWidget;

    vLayout->addWidget(m_spinnerWgt, 5);
    m_spinnerWgt->hide();

    //    m_noResultWdg = new LogSearchNoResultWidget(this);
    //    m_noResultWdg->setContent("");
    //    vLayout->addWidget(m_noResultWdg, 10);
    //    m_noResultWdg->hide();

    vLayout->addLayout(vLy, 3);
    vLayout->setContentsMargins(0, 0, 0, 0);

    this->setLayout(vLayout);

    cleanText();

    DGuiApplicationHelper::ColorType ct = DApplicationHelper::instance()->themeType();
    slot_themeChanged(ct);
}

void DisplayContent::initMap()
{
    m_transDict.clear();
    m_transDict.insert("Warning", DApplication::translate("Level", "warning"));
    m_transDict.insert("Debug", DApplication::translate("Level", "debug"));

    // icon <==> level
    m_icon_name_map.clear();
    m_icon_name_map.insert(DApplication::translate("Level", "Emer"), "warning2.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Alert"), "warning3.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Critical"), "warning2.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Error"), "wrong.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Warning"), "warning.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Notice"), "warning.svg");
    m_icon_name_map.insert(DApplication::translate("Level", "Info"), "");
    m_icon_name_map.insert(DApplication::translate("Level", "Debug"), "");
    m_icon_name_map.insert("Warning", "warning.svg");
    m_icon_name_map.insert("Debug", "");
}

void DisplayContent::cleanText()
{
    m_dateTime->hide();

    m_userName->hide();
    m_userLabel->hide();

    m_pid->hide();
    m_pidLabel->hide();

    m_status->hide();
    m_statusLabel->hide();

    m_level->hide();

    m_daemonName->hide();

    m_textBrowser->clear();
}

void DisplayContent::initTableView()
{
    m_treeView = new LogTreeView(this);

    //    auto changeTheme = [this]() {
    //        DPalette pa = DApplicationHelper::instance()->palette(this);
    //        pa.setColorGroup(QPalette::Inactive, pa.base(), pa.button(), pa.light(), pa.dark(),
    //                         pa.mid(), pa.text(), pa.brightText(), pa.base(), pa.window());
    //        DApplicationHelper::instance()->setPalette(m_tableView, pa);
    //    };

    //    changeTheme();
    //    connect(DApplicationHelper::instance(), &DApplicationHelper::themeTypeChanged, this,
    //            changeTheme);

    m_pModel = new QStandardItemModel(this);

    m_treeView->setModel(m_pModel);
}

void DisplayContent::initConnections()
{
    connect(m_treeView, SIGNAL(clicked(const QModelIndex &)), this,
            SLOT(slot_tableItemClicked(const QModelIndex &)));
    connect(&m_logFileParse, SIGNAL(dpkgFinished()), this, SLOT(slot_dpkgFinished()));
    connect(&m_logFileParse, SIGNAL(xlogFinished()), this, SLOT(slot_XorgFinished()));
    connect(&m_logFileParse, SIGNAL(bootFinished()), this, SLOT(slot_bootFinished()));
    connect(&m_logFileParse, SIGNAL(kernFinished()), this, SLOT(slot_kernFinished()));
    connect(&m_logFileParse, SIGNAL(journalFinished(QList<LOG_MSG_JOURNAL>)), this,
            SLOT(slot_journalFinished(QList<LOG_MSG_JOURNAL>)));
    connect(&m_logFileParse, &LogFileParser::applicationFinished, this,
            &DisplayContent::slot_applicationFinished);
    connect(m_treeView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &DisplayContent::slot_vScrollValueChanged);

    connect(DApplicationHelper::instance(), &DApplicationHelper::themeTypeChanged, this,
            &DisplayContent::slot_themeChanged);
}

void DisplayContent::generateJournalFile(int id, int lId)
{
    jList.clear();

    m_spinnerWgt->spinnerStart();
    m_treeView->hide();
    m_spinnerWgt->show();

    QDateTime dt = QDateTime::currentDateTime();
    dt.setTime(QTime());

    QStringList arg;
    QString prio = QString("PRIORITY=%1").arg(lId);
    arg.append(prio);
    switch (id) {
        case ALL: {
            m_logFileParse.parseByJournal(arg);
        } break;
        case ONE_DAY: {
            arg << "--since" << dt.toString("yyyy-MM-dd");
            m_logFileParse.parseByJournal(arg);
        } break;
        case THREE_DAYS: {
            QString t = dt.addDays(-2).toString("yyyy-MM-dd");
            arg << "--since" << t;
            m_logFileParse.parseByJournal(arg);
        } break;
        case ONE_WEEK: {
            QString t = dt.addDays(-6).toString("yyyy-MM-dd");
            arg << "--since" << t;
            m_logFileParse.parseByJournal(arg);
        } break;
        case ONE_MONTH: {
            QString t = dt.addDays(-29).toString("yyyy-MM-dd");
            arg << "--since" << t;
            m_logFileParse.parseByJournal(arg);
        } break;
        case THREE_MONTHS: {
            QString t = dt.addDays(-89).toString("yyyy-MM-dd");
            arg << "--since" << t;
            m_logFileParse.parseByJournal(arg);
        } break;
        default:
            break;
    }
}

void DisplayContent::createJournalTable(QList<LOG_MSG_JOURNAL> &list)
{
    m_treeView->show();
    noResultLabel->hide();

    m_pModel->clear();

    m_pModel->setColumnCount(6);
    m_pModel->setHorizontalHeaderLabels(QStringList()
                                        << DApplication::translate("Table", "Level")
                                        << DApplication::translate("Table", "Daemon")
                                        << DApplication::translate("Table", "Date and Time")
                                        << DApplication::translate("Table", "Info")
                                        << DApplication::translate("Table", "User")
                                        << DApplication::translate("Table", "PID"));

    int end = list.count() > SINGLE_LOAD ? SINGLE_LOAD : list.count();
    insertJournalTable(list, 0, end);
}

void DisplayContent::generateDpkgFile(int id)
{
    dList.clear();

    m_spinnerWgt->spinnerStop();
    m_treeView->show();
    m_spinnerWgt->hide();

    QDateTime dt = QDateTime::currentDateTime();
    dt.setTime(QTime());  // get zero time
    switch (id) {
        case ALL:
            m_logFileParse.parseByDpkg(dList);
            break;
        case ONE_DAY: {
            m_logFileParse.parseByDpkg(dList, dt.toMSecsSinceEpoch());
        } break;
        case THREE_DAYS: {
            m_logFileParse.parseByDpkg(dList, dt.addDays(-2).toMSecsSinceEpoch());
        } break;
        case ONE_WEEK: {
            m_logFileParse.parseByDpkg(dList, dt.addDays(-6).toMSecsSinceEpoch());
        } break;
        case ONE_MONTH: {
            m_logFileParse.parseByDpkg(dList, dt.addDays(-29).toMSecsSinceEpoch());
        } break;
        case THREE_MONTHS: {
            m_logFileParse.parseByDpkg(dList, dt.addDays(-89).toMSecsSinceEpoch());
        } break;
        default:
            break;
    }
}

void DisplayContent::createDpkgTable(QList<LOG_MSG_DPKG> &list)
{
    m_treeView->show();
    noResultLabel->hide();
    m_pModel->clear();
    m_pModel->setColumnCount(3);
    m_pModel->setHorizontalHeaderLabels(QStringList()
                                        << DApplication::translate("Table", "Date and Time")
                                        << DApplication::translate("Table", "Info")
                                        << DApplication::translate("Table", "Action"));
    DStandardItem *item = nullptr;
    for (int i = 0; i < list.size(); i++) {
        item = new DStandardItem(list[i].dateTime);
        item->setData(DPKG_TABLE_DATA);
        m_pModel->setItem(i, 0, item);
        item = new DStandardItem(list[i].msg);
        item->setData(DPKG_TABLE_DATA);
        m_pModel->setItem(i, 1, item);
        item = new DStandardItem(list[i].action);
        item->setData(DPKG_TABLE_DATA);
        m_pModel->setItem(i, 2, item);
    }
    m_treeView->hideColumn(2);

    m_treeView->setModel(m_pModel);

    // default first row select
    //    m_treeView->selectRow(0);
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::generateKernFile(int id)
{
    kList.clear();
    m_spinnerWgt->spinnerStop();
    m_treeView->show();
    m_spinnerWgt->hide();
    QDateTime dt = QDateTime::currentDateTime();
    dt.setTime(QTime());  // get zero time
    switch (id) {
        case ALL:
            m_logFileParse.parseByKern(kList);
            break;
        case ONE_DAY: {
            m_logFileParse.parseByKern(kList, dt.toMSecsSinceEpoch());
        } break;
        case THREE_DAYS: {
            m_logFileParse.parseByKern(kList, dt.addDays(-2).toMSecsSinceEpoch());
        } break;
        case ONE_WEEK: {
            m_logFileParse.parseByKern(kList, dt.addDays(-6).toMSecsSinceEpoch());
        } break;
        case ONE_MONTH: {
            m_logFileParse.parseByKern(kList, dt.addDays(-29).toMSecsSinceEpoch());
        } break;
        case THREE_MONTHS: {
            m_logFileParse.parseByKern(kList, dt.addDays(-89).toMSecsSinceEpoch());
        } break;
        default:
            break;
    }
}

void DisplayContent::createKernTable(QList<LOG_MSG_JOURNAL> &list)
{
    m_treeView->show();
    noResultLabel->hide();
    m_pModel->clear();
    m_pModel->setColumnCount(4);
    m_pModel->setHorizontalHeaderLabels(QStringList()
                                        << DApplication::translate("Table", "Date and Time")
                                        << DApplication::translate("Table", "User")
                                        << DApplication::translate("Table", "Daemon")
                                        << DApplication::translate("Table", "Info"));
    DStandardItem *item = nullptr;
    for (int i = 0; i < list.size(); i++) {
        item = new DStandardItem(list[i].dateTime);
        item->setData(KERN_TABLE_DATA);
        m_pModel->setItem(i, 0, item);
        item = new DStandardItem(list[i].hostName);
        item->setData(KERN_TABLE_DATA);
        m_pModel->setItem(i, 1, item);
        item = new DStandardItem(list[i].daemonName);
        item->setData(KERN_TABLE_DATA);
        m_pModel->setItem(i, 2, item);
        item = new DStandardItem(list[i].msg);
        item->setData(KERN_TABLE_DATA);
        m_pModel->setItem(i, 3, item);
    }

    m_treeView->setModel(m_pModel);

    // default first row select
    //    m_treeView->selectRow(0);
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::generateAppFile(QString path, int id, int lId)
{
    appList.clear();
    QDateTime dt = QDateTime::currentDateTime();
    dt.setTime(QTime());  // get zero time
    switch (id) {
        case ALL:
            m_logFileParse.parseByApp(path, appList, lId);
            break;
        case ONE_DAY: {
            m_logFileParse.parseByApp(path, appList, lId, dt.toMSecsSinceEpoch());
        } break;
        case THREE_DAYS: {
            m_logFileParse.parseByApp(path, appList, lId, dt.addDays(-2).toMSecsSinceEpoch());
        } break;
        case ONE_WEEK: {
            m_logFileParse.parseByApp(path, appList, lId, dt.addDays(-6).toMSecsSinceEpoch());
        } break;
        case ONE_MONTH: {
            m_logFileParse.parseByApp(path, appList, lId, dt.addDays(-29).toMSecsSinceEpoch());
        } break;
        case THREE_MONTHS: {
            m_logFileParse.parseByApp(path, appList, lId, dt.addDays(-89).toMSecsSinceEpoch());
        } break;
        default:
            break;
    }
}

void DisplayContent::createAppTable(QList<LOG_MSG_APPLICATOIN> &list)
{
    m_treeView->show();
    noResultLabel->hide();
    m_pModel->clear();
    m_pModel->setColumnCount(4);
    m_pModel->setHorizontalHeaderLabels(QStringList()
                                        << DApplication::translate("Table", "Level")
                                        << DApplication::translate("Table", "Date and Time")
                                        << DApplication::translate("Table", "Source")
                                        << DApplication::translate("Table", "Info"));
    DStandardItem *item = nullptr;
    for (int i = 0; i < list.size(); i++) {
        int col = 0;
        QString CH_str = m_transDict.value(list[i].level);
        QString lvStr = CH_str.isEmpty() ? list[i].level : CH_str;
        //        item = new DStandardItem(lvStr);
        item = new DStandardItem();
        QString iconPath = m_iconPrefix + getIconByname(list[i].level);
        item->setIcon(QIcon(iconPath));
        item->setData(APP_TABLE_DATA);
        item->setData(lvStr, Qt::UserRole + 6);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(list[i].dateTime);
        item->setData(APP_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        //        item = new DStandardItem(list[i].src);
        item = new DStandardItem(getAppName(m_curAppLog));
        item->setData(APP_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(list[i].msg);
        item->setData(APP_TABLE_DATA);
        m_pModel->setItem(i, col++, item);
    }

    m_treeView->setModel(m_pModel);

    // default first row select
    //    m_treeView->selectRow(0);
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::createBootTable(QList<LOG_MSG_BOOT> &list)
{
    m_treeView->show();
    noResultLabel->hide();
    m_pModel->clear();
    m_pModel->setColumnCount(2);
    m_pModel->setHorizontalHeaderLabels(QStringList() << DApplication::translate("Table", "Status")
                                                      << DApplication::translate("Table", "Info"));
    DStandardItem *item = nullptr;
    for (int i = 0; i < list.size(); i++) {
        item = new DStandardItem(list[i].status);
        item->setData(BOOT_TABLE_DATA);
        m_pModel->setItem(i, 0, item);
        item = new DStandardItem(list[i].msg);
        item->setData(BOOT_TABLE_DATA);
        m_pModel->setItem(i, 1, item);
    }

    m_treeView->setModel(m_pModel);

    // default first row select
    //    m_treeView->selectRow(0);
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::createXorgTable(QStringList &list)
{
    m_treeView->show();
    noResultLabel->hide();
    m_pModel->clear();
    m_pModel->setColumnCount(1);
    m_pModel->setHorizontalHeaderLabels(QStringList() << DApplication::translate("Table", "Info"));
    DStandardItem *item = nullptr;
    for (int i = 0; i < list.size(); i++) {
        item = new DStandardItem(list[i]);
        item->setData(XORG_TABLE_DATA);
        m_pModel->setItem(i, 0, item);
    }

    m_treeView->setModel(m_pModel);

    // default first row select
    //    m_treeView->selectRow(0);
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::insertJournalTable(QList<LOG_MSG_JOURNAL> logList, int start, int end)
{
    DStandardItem *item = nullptr;
    for (int i = start; i < end; i++) {
        int col = 0;

        item = new DStandardItem();
        //        qDebug() << "journal level" << logList[i].level;
        QString iconPath = m_iconPrefix + getIconByname(logList[i].level);
        item->setIcon(QIcon(iconPath));
        item->setData(JOUR_TABLE_DATA);
        item->setData(logList[i].level, Qt::UserRole + 6);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(logList[i].daemonName);
        item->setData(JOUR_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(logList[i].dateTime);
        item->setData(JOUR_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(logList[i].msg);
        item->setData(JOUR_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(logList[i].hostName);
        item->setData(JOUR_TABLE_DATA);
        m_pModel->setItem(i, col++, item);

        item = new DStandardItem(logList[i].daemonId);
        item->setData(JOUR_TABLE_DATA);
        m_pModel->setItem(i, col++, item);
    }
    m_treeView->hideColumn(4);
    m_treeView->hideColumn(5);

    //    qDebug() << m_pModel->index(0, 0).data(Qt::DecorationRole);

    m_treeView->setModel(m_pModel);

    // default first row select
    QItemSelectionModel *p = m_treeView->selectionModel();
    if (p)
        p->select(m_pModel->index(0, 0), QItemSelectionModel::Rows | QItemSelectionModel::Select);
    slot_tableItemClicked(m_pModel->index(0, 0));
}

void DisplayContent::fillDetailInfo(QString deamonName, QString usrName, QString pid,
                                    QString dateTime, QModelIndex level, QString msg,
                                    QString status)
{
    m_dateTime->show();
    m_daemonName->show();
    m_level->show();
    m_userName->show();
    m_userLabel->show();
    m_pid->show();
    m_pidLabel->show();
    m_status->show();
    m_statusLabel->show();

    deamonName.isEmpty() ? m_daemonName->hide() : m_daemonName->setText(deamonName);

    if (!level.isValid()) {
        m_level->hide();
    } else {
        QIcon icon = m_pModel->item(level.row())->icon();
        m_level->setIcon(icon);
        m_level->setText(m_pModel->item(level.row())->data(Qt::UserRole + 6).toString());
    }

    dateTime.isEmpty() ? m_dateTime->hide() : m_dateTime->setText(dateTime);

    if (usrName.isEmpty()) {
        m_userName->hide();
        m_userLabel->hide();
    } else {
        m_userName->setText(usrName);
    }

    if (pid.isEmpty()) {
        m_pid->hide();
        m_pidLabel->hide();
    } else {
        m_pid->setText(pid);
    }

    if (status.isEmpty()) {
        m_status->hide();
        m_statusLabel->hide();
    } else {
        m_status->setText(status);
    }
    m_textBrowser->setText(msg);
}

QString DisplayContent::getAppName(QString filePath)
{
    QString ret;
    if (filePath.isEmpty())
        return ret;

    QStringList strList = filePath.split("/");
    if (strList.count() < 2) {
        if (filePath.contains("."))
            ret = filePath.section(".", 0, 0);
        else {
            ret = filePath;
        }
        return ret;
    }

    QString desStr = filePath.section("/", -1);
    if (desStr.contains(".")) {
        return desStr.section(".", 0, 0);
    }
    return ret;
}

void DisplayContent::slot_tableItemClicked(const QModelIndex &index)
{
    cleanText();

    if (!index.isValid())
        return;

    // get hostname.
    utsname _utsname;
    uname(&_utsname);
    QString hostname = QString(_utsname.nodename);

    QString dataStr = index.data(Qt::UserRole + 1).toString();

    if (dataStr.contains(DPKG_TABLE_DATA)) {
        fillDetailInfo("Dpkg", hostname, "", m_pModel->item(index.row(), 0)->text(), QModelIndex(),
                       m_pModel->item(index.row(), 1)->text(),
                       m_pModel->item(index.row(), 2)->text());
    } else if (dataStr.contains(XORG_TABLE_DATA)) {
        fillDetailInfo("Xorg", hostname, "", "", QModelIndex(),
                       m_pModel->item(index.row(), 0)->text());
    } else if (dataStr.contains(BOOT_TABLE_DATA)) {
        fillDetailInfo("Boot", hostname, "", "", QModelIndex(),
                       m_pModel->item(index.row(), 1)->text(),
                       m_pModel->item(index.row(), 0)->text());
    } else if (dataStr.contains(KERN_TABLE_DATA)) {
        fillDetailInfo(m_pModel->item(index.row(), 2)->text(),
                       /*m_pModel->item(index.row(), 1)->text()*/ hostname, "",
                       m_pModel->item(index.row(), 0)->text(), QModelIndex(),
                       m_pModel->item(index.row(), 3)->text());
    } else if (dataStr.contains(JOUR_TABLE_DATA)) {
        fillDetailInfo(m_pModel->item(index.row(), 1)->text(),
                       /*m_pModel->item(index.row(), 4)->text()*/ hostname,
                       m_pModel->item(index.row(), 5)->text(),
                       m_pModel->item(index.row(), 2)->text(), index,
                       m_pModel->item(index.row(), 3)->text());
    } else if (dataStr.contains(APP_TABLE_DATA)) {
        fillDetailInfo(getAppName(m_curAppLog), hostname, "",
                       m_pModel->item(index.row(), 1)->text(), index,
                       m_pModel->item(index.row(), 3)->text());
    }
}

void DisplayContent::slot_BtnSelected(int btnId, int lId, QModelIndex idx)
{
    qDebug() << QString("Button %1 clicked, combobox id is %2 tree %3 node!!")
                    .arg(btnId)
                    .arg(lId)
                    .arg(idx.data(Qt::UserRole + 1).toString());

    m_curLvId = lId;
    m_curBtnId = btnId;

    QString treeData = idx.data(Qt::UserRole + 1).toString();
    if (treeData.isEmpty())
        return;

    if (treeData.contains(JOUR_TREE_DATA, Qt::CaseInsensitive)) {
        generateJournalFile(btnId, lId);
    } else if (treeData.contains(DPKG_TREE_DATA, Qt::CaseInsensitive)) {
        generateDpkgFile(btnId);
    } else if (treeData.contains(KERN_TREE_DATA, Qt::CaseInsensitive)) {
        generateKernFile(btnId);
    } else if (treeData.contains(".cache")) {
        //        generateAppFile(treeData, btnId, lId);
    } else if (treeData.contains(APP_TREE_DATA, Qt::CaseInsensitive)) {
        if (!m_curAppLog.isEmpty()) {
            generateAppFile(m_curAppLog, btnId, lId);
        }
    }
}

void DisplayContent::slot_appLogs(QString path)
{
    if (path.isEmpty())
        return;

    appList.clear();
    m_curAppLog = path;
    //    m_logFileParse.parseByApp(path, appList);
    generateAppFile(path, m_curBtnId, m_curLvId);
}

void DisplayContent::slot_treeClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (curIdx == index && (m_flag != KERN && m_flag != BOOT)) {
        qDebug() << "repeat click" << m_flag;
        return;
    }
    curIdx = index;

    cleanText();
    m_pModel->clear();

    QString itemData = index.data(Qt::UserRole + 1).toString();
    if (itemData.isEmpty())
        return;

    if (itemData.contains(JOUR_TREE_DATA, Qt::CaseInsensitive)) {
        // default level is info so PRIORITY=6
        //        jList.clear();
        //        m_logFileParse.parseByJournal(QStringList() << "PRIORITY=6");
        m_flag = JOURNAL;
        generateJournalFile(m_curBtnId, m_curLvId);
    } else if (itemData.contains(DPKG_TREE_DATA, Qt::CaseInsensitive)) {
        //        dList.clear();
        //        m_logFileParse.parseByDpkg(dList);
        m_flag = DPKG;
        generateDpkgFile(m_curBtnId);
    } else if (itemData.contains(XORG_TREE_DATA, Qt::CaseInsensitive)) {
        xList.clear();
        m_flag = XORG;
        m_logFileParse.parseByXlog(xList);
    } else if (itemData.contains(BOOT_TREE_DATA, Qt::CaseInsensitive)) {
        bList.clear();
        m_flag = BOOT;
        m_logFileParse.parseByBoot(bList);
    } else if (itemData.contains(KERN_TREE_DATA, Qt::CaseInsensitive)) {
        //        kList.clear();
        //        m_logFileParse.parseByKern(kList);
        m_flag = KERN;
        generateKernFile(m_curBtnId);
    } else if (itemData.contains(".cache")) {
        //        appList.clear();
        //        m_logFileParse.parseByApp(itemData, appList);
    } else if (itemData.contains(APP_TREE_DATA, Qt::CaseInsensitive)) {
        m_pModel->clear();  // clicked parent node application, clear table contents
        m_flag = APP;
    }

    if (!itemData.contains(JOUR_TREE_DATA, Qt::CaseInsensitive)) {
        m_spinnerWgt->spinnerStop();
        m_treeView->show();
        m_spinnerWgt->hide();
    }
}

void DisplayContent::slot_exportClicked()
{
    QString logName;
    if (curIdx.isValid())
        logName = QString("/%1").arg(curIdx.data().toString());
    else {
        logName = QString("/%1").arg(DApplication::translate("File", "New File"));
    }

    QString selectFilter;
    QString fileName = DFileDialog::getSaveFileName(
        this, DApplication::translate("File", "Export File"), QDir::homePath() + logName + ".txt",
        tr("TEXT (*.txt);; Doc (*.doc);; Xls (*.xls);; Html (*.html)"), &selectFilter);

    if (fileName.isEmpty())
        return;

    QStringList labels;
    for (int col = 0; col < m_pModel->columnCount(); ++col) {
        labels.append(m_pModel->horizontalHeaderItem(col)->text());
    }
    if (selectFilter == "TEXT (*.txt)") {
        if (m_flag != JOURNAL) {
            LogExportWidget::exportToTxt(fileName, m_pModel);
        } else {
            LogExportWidget::exportToTxt(fileName, jList);
        }
    } else if (selectFilter == "Html (*.html)") {
        if (m_flag != JOURNAL) {
            LogExportWidget::exportToHtml(fileName, m_pModel);
        } else {
            LogExportWidget::exportToHtml(fileName, jList);
        }
    } else if (selectFilter == "Doc (*.doc)") {
        if (m_flag != JOURNAL) {
            LogExportWidget::exportToDoc(fileName, m_pModel);
        } else {
            LogExportWidget::exportToDoc(fileName, jList, labels);
        }
    } else if (selectFilter == "Xls (*.xls)") {
        if (m_flag != JOURNAL) {
            LogExportWidget::exportToXls(fileName, m_pModel);
        } else {
            LogExportWidget::exportToXls(fileName, jList, labels);
        }
    }
}

void DisplayContent::slot_dpkgFinished()
{
    if (m_flag != DPKG)
        return;

    createDpkgTable(dList);
}

void DisplayContent::slot_XorgFinished()
{
    if (m_flag != XORG)
        return;

    createXorgTable(xList);
}

void DisplayContent::slot_bootFinished()
{
    if (m_flag != BOOT)
        return;

    createBootTable(bList);
}

void DisplayContent::slot_kernFinished()
{
    if (m_flag != KERN)
        return;

    createKernTable(kList);
}

void DisplayContent::slot_journalFinished(QList<LOG_MSG_JOURNAL> logList)
{
    if (m_flag != JOURNAL)
        return;

    jList = logList;

    m_spinnerWgt->spinnerStop();
    m_treeView->show();
    m_spinnerWgt->hide();

    createJournalTable(logList);
}

void DisplayContent::slot_applicationFinished()
{
    if (m_flag != APP)
        return;

    createAppTable(appList);
}

void DisplayContent::slot_vScrollValueChanged(int value)
{
    if (m_flag != JOURNAL)
        return;

    int rate = (value + 25) / SINGLE_LOAD;
    //    qDebug() << "value: " << value << "rate: " << rate << "single: " << SINGLE_LOAD;

    if (value < SINGLE_LOAD * rate - 20 || value < SINGLE_LOAD * rate) {
        if (m_limitTag == rate)
            return;

        int leftCnt = jList.count() - SINGLE_LOAD * rate;
        int end = leftCnt > SINGLE_LOAD ? SINGLE_LOAD : leftCnt;
        //        qDebug() << "total count: " << jList.count() << "left count : " << leftCnt
        //                 << " start : " << SINGLE_LOAD * rate << "end: " << end + SINGLE_LOAD *
        //                 rate;
        insertJournalTable(jList, SINGLE_LOAD * rate, SINGLE_LOAD * rate + end);

        m_limitTag = rate;
    }
    m_treeView->verticalScrollBar()->setValue(value);
}

void DisplayContent::slot_searchResult(QString str)
{
    qDebug() << QString("search: %1  treeIndex: %2")
                    .arg(str)
                    .arg(curIdx.data(Qt::UserRole + 1).toString());

    if (m_flag == NONE)
        return;

    switch (m_flag) {
        case JOURNAL: {
            QList<LOG_MSG_JOURNAL> tmp = jList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                LOG_MSG_JOURNAL msg = tmp.at(i);
                if (msg.dateTime.contains(str, Qt::CaseInsensitive) ||
                    msg.hostName.contains(str, Qt::CaseInsensitive) ||
                    msg.daemonName.contains(str, Qt::CaseInsensitive) ||
                    msg.daemonId.contains(str, Qt::CaseInsensitive) ||
                    msg.level.contains(str, Qt::CaseInsensitive) ||
                    msg.msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createJournalTable(tmp);
        } break;
        case KERN: {
            QList<LOG_MSG_JOURNAL> tmp = kList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                LOG_MSG_JOURNAL msg = tmp.at(i);
                if (msg.dateTime.contains(str, Qt::CaseInsensitive) ||
                    msg.hostName.contains(str, Qt::CaseInsensitive) ||
                    msg.daemonName.contains(str, Qt::CaseInsensitive) ||
                    msg.msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createKernTable(tmp);
        } break;
        case BOOT: {
            QList<LOG_MSG_BOOT> tmp = bList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                LOG_MSG_BOOT msg = tmp.at(i);
                if (msg.status.contains(str, Qt::CaseInsensitive) ||
                    msg.msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createBootTable(tmp);
        } break;
        case XORG: {
            QStringList tmp = xList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                QString msg = tmp.at(i);
                if (msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createXorgTable(tmp);
        } break;
        case DPKG: {
            QList<LOG_MSG_DPKG> tmp = dList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                LOG_MSG_DPKG msg = tmp.at(i);
                if (msg.dateTime.contains(str, Qt::CaseInsensitive) ||
                    msg.msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createDpkgTable(tmp);
        } break;
        case APP: {
            QList<LOG_MSG_APPLICATOIN> tmp = appList;
            int cnt = tmp.count();
            for (int i = cnt - 1; i >= 0; --i) {
                LOG_MSG_APPLICATOIN msg = tmp.at(i);
                if (msg.dateTime.contains(str, Qt::CaseInsensitive) ||
                    msg.level.contains(str, Qt::CaseInsensitive) ||
                    msg.src.contains(str, Qt::CaseInsensitive) ||
                    msg.msg.contains(str, Qt::CaseInsensitive))
                    continue;
                tmp.removeAt(i);
            }
            createAppTable(tmp);
        } break;
        default:
            break;
    }
    if (0 == m_pModel->rowCount()) {
        m_treeView->hide();

        noResultLabel->show();
        //        m_noResultWdg->setContent(str);
        //        m_noResultWdg->show();
        cleanText();
    } else {
        m_treeView->show();
        noResultLabel->hide();
        //        m_noResultWdg->setContent(str);
        //        m_noResultWdg->hide();
    }
}

void DisplayContent::slot_themeChanged(DGuiApplicationHelper::ColorType colorType)
{
    if (colorType == DGuiApplicationHelper::DarkType) {
        m_iconPrefix = "://images/dark/";
    } else if (colorType == DGuiApplicationHelper::LightType) {
        m_iconPrefix = "://images/light/";
    }
    slot_BtnSelected(m_curBtnId, m_curLvId, curIdx);
}

void DisplayContent::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Save pen
    QPen oldPen = painter.pen();

    painter.setRenderHint(QPainter::Antialiasing);
    DPalette pa = DApplicationHelper::instance()->palette(this);
    painter.setBrush(QBrush(pa.color(DPalette::Base)));
    QColor penColor = pa.color(DPalette::FrameBorder);
    penColor.setAlphaF(0.05);
    painter.setPen(QPen(penColor));

    QRectF rect = this->rect();
    rect.setX(0.5);
    rect.setY(0.5);
    rect.setWidth(rect.width() - 0.5);
    rect.setHeight(rect.height() - 0.5);

    QPainterPath painterPath;
    painterPath.addRoundedRect(rect, 8, 8);
    painter.drawPath(painterPath);

    // Restore the pen
    painter.setPen(oldPen);
}

QString DisplayContent::getIconByname(QString str)
{
    //    qDebug() << str << m_icon_name_map.value(str);
    return m_icon_name_map.value(str);
}
