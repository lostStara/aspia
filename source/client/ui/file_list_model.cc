//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/file_list_model.h"

#include <QDateTime>

#include "host/file_platform_util.h"
#include "client/ui/file_item_mime_data.h"

namespace aspia {

namespace {

enum Column
{
    COLUMN_NAME       = 0,
    COLUMN_SIZE       = 1,
    COLUMN_TYPE       = 2,
    COLUMN_LAST_WRITE = 3,
    COLUMN_COUNT      = 4
};

template<class T>
void sortByName(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(), [order](const T::value_type& f1, const T::value_type& f2)
    {
        const QString& f1_name = f1.name;
        const QString& f2_name = f2.name;

        if (order == Qt::AscendingOrder)
            return f1_name.toLower() > f2_name.toLower();
        else
            return f1_name.toLower() < f2_name.toLower();
    });
}

template<class T>
void sortBySize(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(), [order](const T::value_type& f1, const T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.size > f2.size;
        else
            return f1.size < f2.size;
    });
}

template<class T>
void sortByType(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(), [order](const T::value_type& f1, const T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.type > f2.type;
        else
            return f1.type < f2.type;
    });
}

template<class T>
void sortByTime(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(), [order](const T::value_type& f1, const T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.last_write > f2.last_write;
        else
            return f1.last_write < f2.last_write;
    });
}

} // namespace

FileListModel::FileListModel(QObject* parent)
    : QAbstractItemModel(parent),
      dir_icon_(FilePlatformUtil::directoryIcon()),
      dir_type_(tr("Folder"))
{
    // Nothing
}

void FileListModel::setFileList(const proto::file_transfer::FileList& list)
{
    clear();

    if (!list.item_size())
        return;

    beginInsertRows(QModelIndex(), 0, list.item_size() - 1);

    for (int i = 0; i < list.item_size(); ++i)
    {
        const proto::file_transfer::FileList::Item& item = list.item(i);

        if (item.is_directory())
        {
            Folder folder;
            folder.name       = QString::fromStdString(item.name());
            folder.last_write = item.modification_time();

            folder_items_.append(folder);
        }
        else
        {
            File file;
            file.name       = QString::fromStdString(item.name());
            file.last_write = item.modification_time();
            file.size       = item.size();

            QPair<QIcon, QString> file_info = FilePlatformUtil::fileTypeInfo(file.name);
            file.icon = file_info.first;
            file.type = file_info.second;

            file_items_.append(file);
        }
    }

    endInsertRows();
}

void FileListModel::clear()
{
    if (folder_items_.isEmpty() && file_items_.isEmpty())
        return;

    beginRemoveRows(QModelIndex(), 0, folder_items_.count() + file_items_.count() - 1);

    folder_items_.clear();
    file_items_.clear();

    endRemoveRows();
}

bool FileListModel::isFolder(const QModelIndex& index) const
{
    return index.row() < folder_items_.count();
}

QString FileListModel::nameAt(const QModelIndex& index) const
{
    if (isFolder(index))
        return folder_items_.at(index.row()).name;

    return file_items_.at(index.row() - folder_items_.count()).name;
}

int64_t FileListModel::sizeAt(const QModelIndex& index) const
{
    if (isFolder(index))
        return 0;

    return file_items_.at(index.row() - folder_items_.count()).size;
}

QModelIndex FileListModel::createFolder()
{
    int row = folder_items_.count();

    beginInsertRows(QModelIndex(), row, row);
    folder_items_.append(Folder());
    endInsertRows();

    return createIndex(row, COLUMN_NAME);
}

QModelIndex FileListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex FileListModel::parent(const QModelIndex& /* child */) const
{
    return QModelIndex();
}

int FileListModel::rowCount(const QModelIndex& /* parent */) const
{
    return folder_items_.count() + file_items_.count();
}

int FileListModel::columnCount(const QModelIndex& /* parent */) const
{
    return COLUMN_COUNT;
}

QVariant FileListModel::data(const QModelIndex& index, int role) const
{
    int column = index.column();
    int row = index.row();

    if (!index.isValid() || (folder_items_.count() + file_items_.count()) <= row)
        return QVariant();

    if (isFolder(index))
    {
        const Folder& folder = folder_items_.at(row);

        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (column == COLUMN_NAME)
                    return dir_icon_;
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                switch (column)
                {
                    case COLUMN_NAME:
                        return folder.name;

                    case COLUMN_LAST_WRITE:
                        return timeToString(folder.last_write);

                    case COLUMN_TYPE:
                        return dir_type_;

                    default:
                        break;
                }
            }
            break;

            default:
                break;
        }
    }
    else
    {
        const File& file = file_items_.at(row - folder_items_.count());

        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (column == COLUMN_NAME)
                    return file.icon;
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                switch (column)
                {
                    case COLUMN_NAME:
                        return file.name;

                    case COLUMN_SIZE:
                        return sizeToString(file.size);

                    case COLUMN_TYPE:
                        return file.type;

                    case COLUMN_LAST_WRITE:
                        return timeToString(file.last_write);

                    default:
                        break;
                }
            }
            break;

            default:
                break;
        }
    }

    return QVariant();
}

bool FileListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || (folder_items_.count() + file_items_.count()) <= index.row())
        return false;

    if (role != Qt::EditRole)
        return false;

    if (index.column() != COLUMN_NAME)
        return false;

    QString old_name = nameAt(index);
    QString new_name = value.toString();

    if (old_name.isEmpty())
    {
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        folder_items_.removeLast();
        endRemoveRows();

        emit createFolderRequest(new_name);
    }
    else
    {
        emit nameChangeRequest(old_name, new_name);
    }

    return true;
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();

    switch (section)
    {
        case COLUMN_NAME:
            return tr("Name");

        case COLUMN_SIZE:
            return tr("Size");

        case COLUMN_TYPE:
            return tr("Type");

        case COLUMN_LAST_WRITE:
            return tr("Modified");

        default:
            return QVariant();
    }
}

QStringList FileListModel::mimeTypes() const
{
    return QStringList() << FileItemMimeData::mimeType();
}

QMimeData* FileListModel::mimeData(const QModelIndexList& indexes) const
{
    QList<FileTransfer::Item> file_list;

    for (const auto& index : indexes)
        file_list.append(FileTransfer::Item(nameAt(index), sizeAt(index), isFolder(index)));

    if (file_list.isEmpty())
        return nullptr;

    FileItemMimeData* mime_data = new FileItemMimeData();
    mime_data->setFileList(file_list);
    mime_data->setSource(this);

    return mime_data;
}

bool FileListModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                    int row, int column, const QModelIndex& parent) const
{
    if (!data->hasFormat(FileItemMimeData::mimeType()))
        return false;

    const FileItemMimeData* mime_data = dynamic_cast<const FileItemMimeData*>(data);
    if (!mime_data)
        return false;

    if (mime_data->source() == this)
        return false;

    return true;
}

bool FileListModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                 int row, int column, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    const FileItemMimeData* mime_data = dynamic_cast<const FileItemMimeData*>(data);
    if (!mime_data)
        return false;

    QString folder;
    if (parent.isValid() && isFolder(parent))
        folder = nameAt(parent);

    emit fileListDropped(folder, mime_data->fileList());
    return true;
}

Qt::DropActions FileListModel::supportedDropActions() const
{
    return Qt::CopyAction;
}

Qt::DropActions FileListModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

Qt::ItemFlags FileListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    switch (index.column())
    {
        case COLUMN_NAME:
        {
            if (isFolder(index))
            {
                return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren | Qt::ItemIsEditable |
                    Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
            }
            else
            {
                return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren | Qt::ItemIsEditable |
                    Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
            }
        }

        default:
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
    }
}

void FileListModel::sort(int column, Qt::SortOrder order)
{
    if (current_order_ == order)
        return;

    current_order_ = order;

    switch (column)
    {
        case COLUMN_NAME:
            sortByName(folder_items_, order);
            sortByName(file_items_, order);
            break;

        case COLUMN_SIZE:
            sortBySize(file_items_, order);
            break;

        case COLUMN_TYPE:
            sortByType(file_items_, order);
            break;

        case COLUMN_LAST_WRITE:
            sortByTime(folder_items_, order);
            sortByTime(file_items_, order);
            break;

        default:
            break;
    }

    emit dataChanged(QModelIndex(), QModelIndex());
}

// static
QString FileListModel::sizeToString(int64_t size)
{
    static const int64_t kKB = 1024LL;
    static const int64_t kMB = kKB * 1024LL;
    static const int64_t kGB = kMB * 1024LL;
    static const int64_t kTB = kGB * 1024LL;

    QString units;
    int64_t divider;

    if (size >= kTB)
    {
        units = tr("TB");
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = tr("GB");
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = tr("MB");
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = tr("kB");
        divider = kKB;
    }
    else
    {
        units = tr("B");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

// static
QString FileListModel::timeToString(time_t time)
{
    return QDateTime::fromSecsSinceEpoch(time).toString(Qt::DefaultLocaleShortDate);
}

} // namespace aspia