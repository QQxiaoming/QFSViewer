#include <QDebug>
#include <QIODevice>
#include <QTreeView>
#include <QDateTime>
#include "treemodel.h"
#include "qfonticon.h"

class TreeItem
{
public:
	TreeItem() {}
	TreeItem(QString str, int type, TreeItem *parent) :
        m_str(str),m_type(type),m_size(0),m_timestamp(0),m_pParent(parent)
	{
	}
	~TreeItem()
	{
		qDeleteAll(m_children) ;
	}
	void appendChild(TreeItem *pChild) { m_children.append(pChild) ; }
	void insertChild(int idx, TreeItem *pChild) { m_children.insert(idx, pChild) ; }
	void removeChild(TreeItem *p)
	{
		int index = m_children.indexOf(p) ;
		if ( index < 0 ) { return ; }
		m_children.removeAt(index) ;
		delete p ;
	}

	QString data() { return m_str ; }
	int type() { return m_type ; }
    uint64_t size() { return m_size ; }
    uint32_t timestamp() { return m_timestamp ; }
	void setData(QString str) { m_str = str ; }
	void setType(int type) { m_type = type ; }
	void setSize(uint64_t size) { m_size = size ; }
	void setTimestamp(uint32_t timestamp) { m_timestamp = timestamp ; }
	int childCount() { return m_children.size() ; }
	QList<TreeItem *> &children() { return m_children ; }
	TreeItem *parent() { return m_pParent ; }
	TreeItem *child(int row)
	{
		if ( row < 0 || row >= childCount() ) { return NULL ; }
		return m_children[row] ;
	}
	int row()
	{
		if ( m_pParent ) {
			return m_pParent->children().indexOf(this) ;
		}
		return 0 ;
	}

	void copy(TreeItem *p)
	{
		m_str = p->m_str ;
		for ( int i = 0 ; i < p->m_children.size() ; i ++ ) {
            insertChild(i, new TreeItem(p->m_children[i]->m_str,p->m_children[i]->m_type, this)) ;
			this->m_children[i]->copy(p->m_children[i]) ;
		}
	}

private:
    QString				m_str;
    int				    m_type;
	uint64_t            m_size;
	uint32_t 		 	m_timestamp;
	TreeItem			*m_pParent ;
	QList<TreeItem *>	m_children ;
} ;

TreeModel::TreeModel(QTreeView *parent) :
	QAbstractItemModel(parent),m_roottimestamp(0),m_parent(parent)
{
	m_pRootItem = new TreeItem(" ", 0, NULL) ;
}

TreeModel::~TreeModel()
{
	delete m_pRootItem ;
}

void TreeModel::set_root_timestamp(uint32_t timestamp)
{
	m_roottimestamp = timestamp;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if ( role != Qt::DisplayRole && role != Qt::EditRole  && role != Qt::DecorationRole && role != (Qt::UserRole + 1)) { return QVariant() ; }
	if ( !index.isValid() ) { return QVariant() ; }

    TreeItem *p = static_cast<TreeItem *>(index.internalPointer()) ;
    
	enum fs_entity_type {
		UNKNOWN = 0,
		REG_FILE,
		DIR,
		CHARDEV,
		BLOCKDEV,
		FIFO,
		SOCKET,
		SYMLINK,
		LAST
	};
	struct fs_index {
        int type;
        int column;
		int role;
		QVariant ret;
	} ret[] = {
		{UNKNOWN,  0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf1c0)))},
		{UNKNOWN,  0, Qt::DisplayRole, p->data()},
		{UNKNOWN,  1, Qt::DisplayRole, tr("Root")},
		{UNKNOWN,  2, Qt::DisplayRole, p->childCount()},
		{UNKNOWN,  3, Qt::DisplayRole, QDateTime::fromSecsSinceEpoch(m_roottimestamp).toString("yyyy-MM-dd hh:mm:ss")},
		
		{REG_FILE, 0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf016)))},
		{REG_FILE, 0, Qt::DisplayRole, p->data()},
		{REG_FILE, 1, Qt::DisplayRole, tr("File")}, 
		{REG_FILE, 2, Qt::DisplayRole, [&]() -> QVariant {
											if( p->size() <= 1024) {
												return QString("%1 B").arg(p->size());
											} else if ( p->size() <= 1024 * 1024 ) {
												return QString::number(p->size() / 1024.0, 'f', 2) + QString(" KB");
											} else if ( p->size() <= 1024 * 1024 * 1024 ) {
												return QString::number(p->size() / (1024.0 * 1024.0), 'f', 2) + QString(" MB");
											} else {
												return QString::number(p->size() / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QString(" GB");
											}
										}(),},
		{REG_FILE, 3, Qt::DisplayRole, QDateTime::fromSecsSinceEpoch(p->timestamp()).toString("yyyy-MM-dd hh:mm:ss")},
		
		{DIR,      0, Qt::DecorationRole, [&]() -> QVariant {
												if(m_parent->isExpanded(index)) {
													return QIcon(QFontIcon::icon(QChar(0xf07c)));
												} else {
													return QIcon(QFontIcon::icon(QChar(0xf07b)));
												}
											}(),},
		{DIR,      0, Qt::DisplayRole, p->data()},
		{DIR,      1, Qt::DisplayRole, tr("Directory")},
		{DIR,      2, Qt::DisplayRole, p->childCount()},
		{DIR,      3, Qt::DisplayRole, QDateTime::fromSecsSinceEpoch(p->timestamp()).toString("yyyy-MM-dd hh:mm:ss")},
		
		{CHARDEV,  0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf085)))},
		{CHARDEV,  0, Qt::DisplayRole, p->data()},
		{CHARDEV,  1, Qt::DisplayRole, tr("Device")},

		{BLOCKDEV, 0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf085)))},
		{BLOCKDEV, 0, Qt::DisplayRole, p->data()},
		{BLOCKDEV, 1, Qt::DisplayRole, tr("Device")},

		{FIFO,     0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf0c1)))},
		{FIFO,     0, Qt::DisplayRole, p->data()},
		{FIFO,     1, Qt::DisplayRole, tr("Link")},

		{SOCKET,   0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf0c1)))},
		{SOCKET,   0, Qt::DisplayRole, p->data()},
		{SOCKET,   1, Qt::DisplayRole, tr("Link")},

		{SYMLINK,  0, Qt::DecorationRole, QIcon(QFontIcon::icon(QChar(0xf0c1)))},
		{SYMLINK,  0, Qt::DisplayRole, p->data()},
		{SYMLINK,  1, Qt::DisplayRole, tr("Link")},
	};

    for (size_t i = 0; i < sizeof(ret) / sizeof(ret[0]); i++) {
		if (p->type() == ret[i].type && index.column() == ret[i].column && role == ret[i].role) {
			return ret[i].ret;
		}
	}

	if(index.column() == 0 && role == Qt::DecorationRole) {
		return QIcon(QFontIcon::icon(QChar(0xf071)));
	} else if(index.column() == 0 && role == Qt::DisplayRole) {
		return p->data();
	} else if (index.column() == 1 && role == Qt::DisplayRole) {
		return tr("Unknown");
	}

	return QVariant();
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
	TreeItem *p = m_pRootItem ;
	if ( parent.isValid() ) {
		p = static_cast<TreeItem *>(parent.internalPointer()) ;
	}

	return p->childCount() ;
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
	return 4 ;	// カラムは常に1つ
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
	if ( !index.isValid() ) {
		return Qt::ItemIsEnabled ;
	}
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
		 | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled ;		// drag and drop処理入れる時は追加
}

bool TreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if ( role != Qt::DisplayRole && role != Qt::EditRole ) {
		return false ;
	}

	TreeItem *p = m_pRootItem ;
	if ( index.isValid() ) {
		p = static_cast<TreeItem *>(index.internalPointer()) ;
	}

    p->setData(value.toList()[0].toString()) ;
    p->setType(value.toList()[1].toInt()) ;
    p->setSize(value.toList()[2].toULongLong()) ;
	p->setTimestamp(value.toList()[3].toUInt()) ;
	emit dataChanged(index, index);
	return true ;
}

bool TreeModel::insertRows(int row, int count, const QModelIndex &parent)
{
	beginInsertRows(parent, row, row+count-1) ;
	TreeItem *p = m_pRootItem ;
	if ( parent.isValid() ) {
		p = static_cast<TreeItem *>(parent.internalPointer()) ;
	}

    p->insertChild(row, new TreeItem(QString(), 0, p)) ;
	endInsertRows();
	return true ;
}

bool TreeModel::removeRows(int row, int count, const QModelIndex &parent)
{
	beginRemoveRows(parent, row, row+count-1) ;
	TreeItem *p = m_pRootItem ;
	if ( parent.isValid() ) {
		p = static_cast<TreeItem *>(parent.internalPointer()) ;
	}

	p->removeChild(p->child(row)) ;
	endRemoveRows();
	return true ;
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
	if ( !hasIndex(row, column, parent) ) { return QModelIndex() ; }

	TreeItem *p = m_pRootItem ;
	if ( parent.isValid() ) {
		p = static_cast<TreeItem *>(parent.internalPointer()) ;
	}

	TreeItem *pChild = p->child(row) ;
	if ( pChild ) {
		return createIndex(row, column, pChild) ;
	}
	return QModelIndex() ;
}

QModelIndex TreeModel::parent(const QModelIndex &child) const
{
	if ( !child.isValid() ) { return QModelIndex() ; }
	TreeItem *c = static_cast<TreeItem *>(child.internalPointer()) ;
	TreeItem *p = c->parent() ;
	if ( p == m_pRootItem ) { return QModelIndex() ; }
	return createIndex(p->row(), 0, p) ;
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch(section) {
			case 0:
				return tr("Name");
			case 1:
				return tr("Kind");
			case 2:
				return tr("Size");
			case 3:
				return tr("Date");
		}
	}
	return QVariant() ;
}

// drag and drop 処理 ----------------------------------------
Qt::DropActions TreeModel::supportedDropActions() const
{
	return Qt::CopyAction | Qt::MoveAction ;
}

QStringList TreeModel::mimeTypes() const
{
	QStringList types ;
	types << "application/tree.item.list" ;
	return types ;
}

QMimeData *TreeModel::mimeData(const QModelIndexList &indexes) const
{
	QMimeData *mimeData = new QMimeData() ;
	QByteArray encodeData ;

	QDataStream stream(&encodeData, QIODevice::WriteOnly) ;
	foreach ( const QModelIndex &index, indexes ) {
		if ( index.isValid() ) {
			stream << reinterpret_cast<quint64>(index.internalPointer()) ;
		}
	}
	mimeData->setData("application/tree.item.list", encodeData) ;
	return mimeData ;
}

bool TreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(row);

	if ( action == Qt::IgnoreAction ) { return true ; }
	if ( !data->hasFormat("application/tree.item.list") ) { return false ; }
	if ( column > 0 ) { return false ; }

	QByteArray encodeData = data->data("application/tree.item.list") ;
	QDataStream stream(&encodeData, QIODevice::ReadOnly) ;

	while ( !stream.atEnd() ) {
		quint64 val ;
		TreeItem *p ;
		stream >> val ;
		p = reinterpret_cast<TreeItem *>(val) ;

		QString text = p->data() ;
        QModelIndex index = addTree(text, 0, 0, 0, parent) ;
		TreeItem *newItem = static_cast<TreeItem *>(index.internalPointer()) ;
		newItem->copy(p) ;
	}
	return true;
}
// drag and drop 処理 ここまで ----------------------------------

QModelIndex TreeModel::addTree(QString str, int type, uint64_t size, uint32_t timestamp, const QModelIndex &parent)
{
	TreeItem *p = m_pRootItem ;
	if ( parent.isValid() ) {
		p = static_cast<TreeItem *>(parent.internalPointer()) ;
	}
	int row = p->childCount() ;

	insertRows(row, 1, parent) ;	// row 追加

    QModelIndex index = this->index(row, 0, parent) ;
    QList<QVariant> list = {str, type, static_cast<quint64>(size), timestamp };
    setData(index, list, Qt::DisplayRole) ;
	return index ;
}

void TreeModel::removeTree(QModelIndex &index)
{
	if ( !index.isValid() ) { return ; }

	removeRows(index.row(), 1, index.parent()) ;
}

void TreeModel::dumpTreeItems()
{
	TreeItem *p = m_pRootItem ;
	int tab = 0 ;
	qDebug() << p->data() ;
	for ( int i = 0 ; i < p->childCount() ; i ++ ) {
		_dump(p->child(i), tab + 1) ;
	}
	qDebug() << "dump end---------" ;
}

void TreeModel::_dump(TreeItem *p, int tab)
{
	QString t ;
	for ( int i = 0 ; i < tab ; i ++ ) {
		t += " " ;
	}
	t += p->data() ;
	qDebug() << t ;

	for ( int i = 0 ; i < p->childCount() ; i ++ ) {
		_dump(p->child(i), tab + 1) ;
	}
}


