#include "libtreehash.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStringList>
#include <QSet>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <unistd.h>

constexpr QCryptographicHash::Algorithm HASH_ALGORITHM = QCryptographicHash::Algorithm::Keccak_512;

using namespace TreeHash;

namespace TreeHash {
class LibTreeHashPrivate{
public:

    EventListener eventListener;
    QFile hashFile;
    QStringList files;
    QString hmacKey;

    QJsonObject hashes;

    bool loadHashes();
    bool storeHashes();

    QString computeFileHash(QString path);
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
        // try creating a new file
        QFile newFile(path);
        if(!newFile.open(QFile::OpenModeFlag::NewOnly | QFile::OpenModeFlag::WriteOnly))
            throw std::invalid_argument(QStringLiteral("file for HashesFile does not exist an can not be created (%1)").arg(path).toStdString());
    }

    QFile& file = this->priv->hashFile;
    if(file.isOpen())
        file.close();
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

void LibTreeHash::setHmacKey(QString hmac){
    this->priv->hmacKey = hmac;
}

QString LibTreeHash::getHmacKey(){
    return this->priv->hmacKey;
}

void LibTreeHash::run(){
    // open file
    auto openFlags = this->runMode == RunMode::VERIFY ? QFile::OpenModeFlag::ReadOnly
                                                      : (QFile::OpenModeFlag::ReadWrite | QFile::OpenModeFlag::Append);
    QFile& hashFile = this->priv->hashFile;
    if(!hashFile.open(openFlags)){
        QString fileErr = hashFile.errorString();
        this->priv->eventListener.callOnError(QStringLiteral("unable to load hashes: can not open file (%1)").arg(fileErr),
                                              QStringLiteral("loading hashes"));
        return;
    }

    if(!this->priv->loadHashes())
        return;

    QDir root(this->rootDir);
    if(!root.exists()){
        this->priv->eventListener.callOnWarning(QStringLiteral("the root-dir dies not exist"), QStringLiteral("run"));
    }

    QFileInfo fi;
    QString hash, relPath;
    for(QString& f : this->priv->files){
        // check if is file
        fi.setFile(f);
        if(!fi.isFile()){
            this->priv->eventListener.callOnWarning(QStringLiteral("item on file-list is not a file; skipping"), f);
            this->priv->eventListener.callOnFileProcessed(f, false);
            continue;
        }

        // create relative path
        relPath = root.relativeFilePath(f);
        if(relPath.contains(QStringLiteral("../"))){
            this->priv->eventListener.callOnWarning(QStringLiteral("file is not in root-dir or its subdirs"), f);
        }

        // compute hash
        hash = this->priv->computeFileHash(f);
        if(hash.isNull()){
            this->priv->eventListener.callOnFileProcessed(f, false);
            continue;
        }

        switch (this->runMode) {
            case RunMode::VERIFY:{
                // compare with list
                auto entry = this->priv->hashes.value(relPath);
                if(entry.isUndefined()){
                    this->priv->eventListener.callOnWarning(QStringLiteral("file has no saved hash; skipping"), f);
                    this->priv->eventListener.callOnFileProcessed(f, false);
                }else{
                    if(!entry.isString()){
                        this->priv->eventListener.callOnError(QStringLiteral("stored hash is not of type string; skipping"), f);
                        this->priv->eventListener.callOnFileProcessed(f, false);
                    }else{
                        bool matches = hash == entry.toString();
                        this->priv->eventListener.callOnFileProcessed(f, matches);
                    }
                }
                break;
            }
            case RunMode::UPDATE:{
                this->priv->hashes.insert(relPath, QJsonValue(hash));
                this->priv->eventListener.callOnFileProcessed(f, true);
                break;
            }
        }
    }

    if(this->runMode == RunMode::UPDATE){
        this->priv->storeHashes();
    }
}

bool LibTreeHashPrivate::loadHashes(){
    if(this->hashFile.size() == 0){
        // new file -> empty JsonObject
        this->hashes = QJsonObject();
        return true;
    }else{
        this->hashFile.seek(0);
        QJsonParseError parseErr;
        QJsonDocument json = QJsonDocument::fromJson(this->hashFile.readAll(), &parseErr);

        if(parseErr.error != QJsonParseError::NoError){
            this->eventListener.callOnError(QStringLiteral("unable to load hashes: file is malformed (invalid JSON)"),
                                            QStringLiteral("loading hashes"));
            return false;
        }

        if(!json.isObject()){
            this->eventListener.callOnError(QStringLiteral("unable to load hashes: file is malformed (expected JSON-Object)"),
                                            QStringLiteral("loading hashes"));
            return false;
        }

        this->hashes = json.object();
        return true;
    }
}

bool LibTreeHashPrivate::storeHashes(){
    QJsonDocument json(this->hashes);
    QByteArray jsonData = json.toJson(QJsonDocument::JsonFormat::Indented);

    // rewrite file
    QFile& file = this->hashFile;
    if(!file.resize(0)){
        this->eventListener.callOnError(QStringLiteral("unable to save hashes: ") + file.errorString(),
                                        QStringLiteral("saving hashes"));
        return false;
    }
    if(file.write(jsonData) == -1){
        this->eventListener.callOnError(QStringLiteral("unable to save hashes: ") + file.errorString(),
                                        QStringLiteral("saving hashes"));
        return false;
    }

    file.flush();
    fsync(file.handle());// without this the tests read only an empty file

    return true;
}

QString LibTreeHashPrivate::computeFileHash(QString path){
    QFile file(path);
    if(!file.open(QFile::OpenModeFlag::ReadOnly | QFile::OpenModeFlag::ExistingOnly)){
        this->eventListener.callOnError(QStringLiteral("unable to read file (%1)").arg(file.errorString()), path);
        return QString();
    }

    if(this->hmacKey.isEmpty()){
        // normal hash
        QCryptographicHash hash(HASH_ALGORITHM);
        if(!hash.addData(&file)){
            this->eventListener.callOnError(QStringLiteral("unable to read file"), path);
            return QString();
        }

        return QString(hash.result().toHex());
    }else{
        // use HMAC
        QMessageAuthenticationCode hash(HASH_ALGORITHM);
        hash.setKey(this->hmacKey.toUtf8());
        if(!hash.addData(&file)){
            this->eventListener.callOnError(QStringLiteral("unable to read file"), path);
            return QString();
        }

        return QString(hash.result().toHex());
    }
}

QStringList TreeHash::listAllFilesInDir(const QString root, bool includeLinkedDirs, bool includeLinkedFiles)
{
    if(!QFileInfo(root).isDir()){
        throw std::invalid_argument(QStringLiteral("given path is not a directory").toStdString());
    }

    QDirIterator::IteratorFlags iterFlags = QDirIterator::IteratorFlag::Subdirectories;
    if(includeLinkedDirs)
        iterFlags |= QDirIterator::IteratorFlag::FollowSymlinks;
    QDir::Filters iterFilter = QDir::Filter::AllEntries | QDir::Filter::Hidden | QDir::Filter::NoDotAndDotDot;
    QDirIterator iter(root, iterFilter, iterFlags);

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

void TreeHash::cleanHashFile(const QString hashfilePath, const QString rootPath, QStringList keep, QString* error){
    QDir rootDir(rootPath);
    if(!rootDir.exists()){
        if(error != nullptr)
            *error = QStringLiteral("root does not exist");
        return;
    }

    // read hashes
    QFile hashFile(hashfilePath);
    if(!hashFile.open(QFile::OpenModeFlag::ReadWrite)){
        if(error != nullptr)
            *error = QStringLiteral("unable to open hash-file: ") + hashFile.errorString();
        return;
    }
    if(!hashFile.seek(0)){
        if(error != nullptr)
            *error = QStringLiteral("unable to open hash-file: ") + hashFile.errorString();
        return;
    }

    QJsonObject hashes;
    if(hashFile.size() == 0){
        // new file -> empty JsonObject
        hashes = QJsonObject();
    }else{
        QJsonParseError parseErr;
        QJsonDocument json = QJsonDocument::fromJson(hashFile.readAll(), &parseErr);

        if(parseErr.error != QJsonParseError::NoError){
            if(error != nullptr)
                *error = QStringLiteral("unable to load hashes: file is malformed (invalid JSON)");
            return;
        }
        if(!json.isObject()){
            if(error != nullptr)
                *error = QStringLiteral("unable to load hashes: file is malformed (invalid JSON)");
            return;
        }

        hashes = json.object();
    }

    // filter hashes
    QSet<QString> keepSet(keep.begin(), keep.end());
    QJsonObject filteredHashes;
    for(const QString& f : hashes.keys()){
        QString absPath = rootDir.absoluteFilePath(f);
        if(keepSet.contains(f) || keepSet.contains(absPath)){
            filteredHashes.insert(f, hashes.value(f));
        }
    }

    // save hashes
    QJsonDocument json(filteredHashes);
    QByteArray jsonData = json.toJson(QJsonDocument::JsonFormat::Indented);

    if(!hashFile.resize(0)){
        if(error != nullptr)
            *error = QStringLiteral("unable to save hashes: ") + hashFile.errorString();
        return;
    }
    if(hashFile.write(jsonData) == -1){
        if(error != nullptr)
            *error = QStringLiteral("unable to save hashes: ") + hashFile.errorString();
        return;
    }

    hashFile.flush();
    fsync(hashFile.handle());// without this the tests read only an empty file

    if(error != nullptr)
        *error = QString();
}

QStringList TreeHash::checkForRemovedFiles(const QString hashfilePath, const QString rootPath, const QStringList files, QString* error){
    QDir rootDir(rootPath);
    if(!rootDir.exists()){
        if(error != nullptr)
            *error = QStringLiteral("root does not exist");
        return QStringList();
    }

    // read hashes
    QFile hashFile(hashfilePath);
    if(!hashFile.open(QFile::OpenModeFlag::ReadWrite)){
        if(error != nullptr)
            *error = QStringLiteral("unable to open hash-file: ") + hashFile.errorString();
        return QStringList();
    }
    if(!hashFile.seek(0)){
        if(error != nullptr)
            *error = QStringLiteral("unable to open hash-file: ") + hashFile.errorString();
        return QStringList();
    }

    QJsonObject hashes;
    if(hashFile.size() == 0){
        // new file -> empty JsonObject
        hashes = QJsonObject();
    }else{
        QJsonParseError parseErr;
        QJsonDocument json = QJsonDocument::fromJson(hashFile.readAll(), &parseErr);

        if(parseErr.error != QJsonParseError::NoError){
            if(error != nullptr)
                *error = QStringLiteral("unable to load hashes: file is malformed (invalid JSON)");
            return QStringList();
        }
        if(!json.isObject()){
            if(error != nullptr)
                *error = QStringLiteral("unable to load hashes: file is malformed (invalid JSON)");
            return QStringList();
        }

        hashes = json.object();
    }

    // find all removed files
    QStringList removed;
    for(const QString& file : hashes.keys()){
        QString absPath = rootDir.absoluteFilePath(file);
        if(!files.contains(absPath)){
            removed.append(file);
        }
    }

    if(error != nullptr)
        *error = QString();
    return removed;
}
