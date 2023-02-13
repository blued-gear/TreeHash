#include "libtreehash.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStringList>

using namespace TreeHash;

namespace TreeHash {
class LibTreeHashPrivate{
public:

    EventListener eventListener;
    QFile hashFile;
    QStringList files;
};
}

LibTreeHash::LibTreeHash(const EventListener& listener)
{
    this->priv = new LibTreeHashPrivate();
    this->priv->eventListener = listener;
}
LibTreeHash::LibTreeHash(LibTreeHash&& mve){
    this->priv = mve.priv;
    mve.priv = nullptr;
    this->runMode = mve.runMode;
}
LibTreeHash::~LibTreeHash(){
    delete this->priv;
}

LibTreeHash& LibTreeHash::operator=(LibTreeHash&& mve){
    this->priv = mve.priv;
    mve.priv = nullptr;
    this->runMode = mve.runMode;
    return *this;
}

void LibTreeHash::setHashesFile(const QString path)
{
    // check if path is a readable file
    if(!QFileInfo::exists(path)){
        throw std::invalid_argument(QStringLiteral("file for HashesFile dies not exist (%1)").arg(path).toStdString());
    }

    QFile& file = this->priv->hashFile;
    file.setFileName(path);
}

const QString LibTreeHash::getHashesFile()
{
    return this->priv->hashFile.fileName();
}

void LibTreeHash::setFiles(const QStringList paths)
{
    this->priv->files = paths;
}

const QStringList LibTreeHash::getFiles()
{
    return this->priv->files;
}

}

QStringList listAllFilesInDir(const QString root, bool includeLinkedDirs, bool includeLinkedFiles)
{
    if(!QFileInfo(root).isDir()){
        throw std::invalid_argument(QStringLiteral("given path is not a directory").toStdString());
    }

    QFlags<QDirIterator::IteratorFlag> iterFlags = QDirIterator::IteratorFlag::Subdirectories;
    if(includeLinkedDirs)
        iterFlags |= QDirIterator::IteratorFlag::FollowSymlinks;
    QDirIterator iter(root, iterFlags);

    QStringList ret;
    while(iter.hasNext()){
        QString e = iter.next();
        QFileInfo fi(e);
        if(!fi.isFile()) continue;
        if(!includeLinkedFiles && fi.isSymLink()) continue;
        ret.append(e);
    }

    return ret;
}
