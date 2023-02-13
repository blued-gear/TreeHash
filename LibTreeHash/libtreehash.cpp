#include "libtreehash.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStringList>
#include <QSet>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <unistd.h>

using namespace TreeHash;

namespace TreeHash {
class LibTreeHashPrivate{
public:

    EventListener eventListener;
    QFile hashFile;
    QStringList files;
    QString hmacKey;
    QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Algorithm::Keccak_512;

    QJsonObject hashes;

    bool storeHashes();

    void verifyEntry(const QString& file, const QString& relPath);
    void updateEntry(const QString& file, const QString& relPath);

    void openHashFile(QString& rootDir);
    void processFiles(RunMode runMode, QString& rootDir);
    QString computeFileHash(QString path);

    static QJsonObject loadHashes(QFile& hashFile, QString* error);
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

void LibTreeHash::setHashAlgorithm(QCryptographicHash::Algorithm alg){
    this->priv->hashAlgorithm = alg;
}

QCryptographicHash::Algorithm LibTreeHash::getHashAlgorithm() const{
    return this->priv->hashAlgorithm;
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

const QString LibTreeHash::getHashesFile() const
{
    return this->priv->hashFile.fileName();
}

void LibTreeHash::setFiles(const QStringList paths)
{
    this->priv->files = paths;
}

const QStringList LibTreeHash::getFiles() const
{
    return this->priv->files;
}

void LibTreeHash::setHmacKey(QString hmac){
    this->priv->hmacKey = hmac;
}

QString LibTreeHash::getHmacKey() const{
    return this->priv->hmacKey;
}

void LibTreeHash::run(){//TODO use cleaned paths (without '.') in log-messages
    this->priv->openHashFile(this->rootDir);

    this->priv->processFiles(this->runMode, this->rootDir);

    if(this->runMode == RunMode::UPDATE || this->runMode == RunMode::UPDATE_NEW){
        this->priv->storeHashes();
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

void LibTreeHashPrivate::verifyEntry(const QString& file, const QString& relPath){
    // compute hash
    QString hash = this->computeFileHash(file);
    if(hash.isNull()){
        this->eventListener.callOnFileProcessed(file, false);
        return;
    }

    // compare with list
    auto entry = this->hashes.value(relPath);
    if(entry.isUndefined()){
        this->eventListener.callOnWarning(QStringLiteral("file has no saved hash; skipping"), file);
        this->eventListener.callOnFileProcessed(file, false);
    }else{
        if(!entry.isString()){
            this->eventListener.callOnError(QStringLiteral("stored hash is not of type string; skipping"), file);
            this->eventListener.callOnFileProcessed(file, false);
        }else{
            bool matches = hash == entry.toString();
            this->eventListener.callOnFileProcessed(file, matches);
        }
    }
}

void LibTreeHashPrivate::updateEntry(const QString& file, const QString& relPath){
    QString hash = this->computeFileHash(file);
    if(hash.isNull()){
        this->eventListener.callOnFileProcessed(file, false);
        return;
    }

    this->hashes.insert(relPath, QJsonValue(hash));

    this->eventListener.callOnFileProcessed(file, true);
}

void LibTreeHashPrivate::openHashFile(QString& rootDir){
    QDir root(rootDir);
    if(!root.exists()){
        this->eventListener.callOnWarning(QStringLiteral("the root-dir dies not exist"), QStringLiteral("run"));
    }

    QString loadError;
    this->hashes = LibTreeHashPrivate::loadHashes(this->hashFile, &loadError);
    if(!loadError.isNull()){
        this->eventListener.callOnError(loadError, QStringLiteral("loading hashes"));
        return;
    }
}

void LibTreeHashPrivate::processFiles(RunMode runMode, QString& rootDir){
    const QDir root(rootDir);

    QFileInfo fi;
    QString relPath;
    for(QString& f : this->files){
        // check if is a file
        fi.setFile(f);
        if(!fi.isFile()){
            this->eventListener.callOnWarning(QStringLiteral("item on file-list is not a file; skipping"), f);
            this->eventListener.callOnFileProcessed(f, false);
            continue;
        }

        // create relative path
        relPath = root.relativeFilePath(f);
        if(relPath.contains(QStringLiteral("../"))){
            this->eventListener.callOnWarning(QStringLiteral("file is not in root-dir or its subdirs"), f);
        }

        switch (runMode) {
            case RunMode::VERIFY: {
                this->verifyEntry(f, relPath);
                break;
            }
            case RunMode::UPDATE: {
                this->updateEntry(f, relPath);
                break;
            }
            case RunMode::UPDATE_NEW: {
                if(!this->hashes.contains(relPath)){
                    this->updateEntry(f, relPath);
                }
            }
        }
    }
}

QString LibTreeHashPrivate::computeFileHash(QString path){
    QFile file(path);
    if(!file.open(QFile::OpenModeFlag::ReadOnly | QFile::OpenModeFlag::ExistingOnly)){
        this->eventListener.callOnError(QStringLiteral("unable to read file (%1)").arg(file.errorString()), path);
        return QString();
    }

    if(this->hmacKey.isEmpty()){
        // normal hash
        QCryptographicHash hash(this->hashAlgorithm);
        if(!hash.addData(&file)){
            this->eventListener.callOnError(QStringLiteral("unable to read file"), path);
            return QString();
        }

        return QString(hash.result().toHex());
    }else{
        // use HMAC
        QMessageAuthenticationCode hash(this->hashAlgorithm);
        hash.setKey(this->hmacKey.toUtf8());
        if(!hash.addData(&file)){
            this->eventListener.callOnError(QStringLiteral("unable to read file"), path);
            return QString();
        }

        return QString(hash.result().toHex());
    }
}

QJsonObject LibTreeHashPrivate::loadHashes(QFile& hashFile, QString* error){
    if(!hashFile.open(QFile::OpenModeFlag::ReadWrite)){
        if(error != nullptr){
            QString fileErr = hashFile.errorString();
            *error = QStringLiteral("unable to load hashes: can not open file (%1)").arg(fileErr);
        }

        return QJsonObject();
    }
    if(!hashFile.seek(0)){
        if(error != nullptr){
            QString fileErr = hashFile.errorString();
            *error =  QStringLiteral("unable to load hashes: can not seek in file (%1)").arg(fileErr);
        }

        return QJsonObject();
    }

    if(hashFile.size() == 0){
        // new file -> empty JsonObject
        return QJsonObject();
    }else{
        hashFile.seek(0);
        QJsonParseError parseErr;
        QJsonDocument json = QJsonDocument::fromJson(hashFile.readAll(), &parseErr);

        if(parseErr.error != QJsonParseError::NoError){
            *error = "unable to load hashes: file is malformed (invalid JSON)";

            return QJsonObject();
        }

        if(!json.isObject()){
            *error = "unable to load hashes: file is malformed (expected JSON-Object)";

            return QJsonObject();
        }

        return json.object();
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
        ret.append(fi.absoluteFilePath());
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

    QFile hashFile(hashfilePath);
    QString loadError;
    QJsonObject hashes = LibTreeHashPrivate::loadHashes(hashFile, &loadError);
    if(!loadError.isNull()){
        *error = loadError;
        return;
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

    QFile hashFile(hashfilePath);
    QString loadError;
    QJsonObject hashes = LibTreeHashPrivate::loadHashes(hashFile, &loadError);
    if(!loadError.isNull()){
        *error = loadError;
        return QStringList();
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
