#include "libtreehash.h"
#include <QFileDevice>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStringList>
#include <QSet>
#include <QMessageAuthenticationCode>
#include <unistd.h>
#include "ext/nlohmann/json.hpp"

using namespace TreeHash;
using namespace nlohmann;

namespace TreeHash {
class LibTreeHashPrivate{
public:

    EventListener eventListener;
    std::unique_ptr<QFileDevice> hashFileSrc, hashFileDst;
    bool truncateHashFileDst = true;
    QStringList files;
    QString hmacKey;
    QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Algorithm::Keccak_512;

    json hashes;

    bool storeHashes();

    void verifyEntry(const QString& file, const QString& relPath);
    void updateEntry(const QString& file, const QString& relPath);

    void openHashFile(QString& rootDir);
    void processFiles(RunMode runMode, QString& rootDir);
    QString computeFileHash(QString path);

    static json loadHashes(QFileDevice& hashFile, QString* error);

    /**
     * @brief if file is open checks if it is readable / writeable;
     *          if it is not open it will try to open it with the appropriate mode
     * @param file the file to check
     * @param write if true make sure the file is writeable
     * @param error if not nullptr an error-message will be stored on failure
     * @return true if file is open and readable / writeable, false otherwise
     */
    static bool ensureFileOpen(QFileDevice& file, bool write, QString* error);
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

void LibTreeHash::setHashesFilePath(const QString path){
    if(!QFileInfo::exists(path)){
        QFile file(path);
        // it may not exist -> try creating a new file
        if(!file.open(QFile::OpenModeFlag::NewOnly | QFile::OpenModeFlag::WriteOnly))
            throw std::invalid_argument(QStringLiteral("file for HashesFile does not exist and can not be created (%1)")
                                            .arg(file.errorString()).toStdString()
            );
    }

    auto srcFile = std::unique_ptr<QFileDevice>(new QFile(path));
    auto dstFile = std::unique_ptr<QFileDevice>(new QFile(path));
    setHashesFile(std::move(srcFile), std::move(dstFile));
}

void LibTreeHash::setHashesFile(std::unique_ptr<QFileDevice>&& src, std::unique_ptr<QFileDevice>&& dst, bool truncateDest)
{
    if(this->priv->hashFileSrc != nullptr && this->priv->hashFileSrc->isOpen()){
        this->priv->hashFileSrc->close();
    }
    if(this->priv->hashFileDst != nullptr && this->priv->hashFileDst->isOpen()){
        this->priv->hashFileDst->close();
    }

    this->priv->hashFileSrc = std::move(src);
    this->priv->hashFileDst = std::move(dst);
    this->priv->truncateHashFileDst = truncateDest;
}

const QFileDevice& LibTreeHash::getHashesFileSrc() const
{
    return *this->priv->hashFileSrc;
}

const QFileDevice& LibTreeHash::getHashesFileDst() const{
    return *this->priv->hashFileDst;
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

void LibTreeHash::run(){
    QString openError;
    if(!LibTreeHashPrivate::ensureFileOpen(*this->priv->hashFileSrc, false, &openError)){
        throw std::invalid_argument(QStringLiteral("unable to open HashesFile source: %1").arg(openError).toStdString());
    }

    if(this->runMode == RunMode::UPDATE || this->runMode == RunMode::UPDATE_NEW){
        if(!LibTreeHashPrivate::ensureFileOpen(*this->priv->hashFileDst, true, &openError)){
            throw std::invalid_argument(QStringLiteral("unable to open HashesFile destination: %1").arg(openError).toStdString());
        }
    }

    this->priv->openHashFile(this->rootDir);

    this->priv->processFiles(this->runMode, this->rootDir);

    if(this->runMode == RunMode::UPDATE || this->runMode == RunMode::UPDATE_NEW){
        this->priv->storeHashes();
    }
}

bool LibTreeHashPrivate::storeHashes(){
    std::string jsonData = this->hashes.dump(4);

    // rewrite file
    QFileDevice& file = *this->hashFileDst;

    if(this->truncateHashFileDst){
        if(!file.resize(0)){
            this->eventListener.callOnError(QStringLiteral("unable to save hashes (truncate): ") + file.errorString(),
                                            QStringLiteral("saving hashes"));
            return false;
        }
    }

    if(file.write(jsonData.c_str()) == -1){
        this->eventListener.callOnError(QStringLiteral("unable to save hashes (write): ") + file.errorString(),
                                        QStringLiteral("saving hashes"));
        return false;
    }

    file.flush();
    fsync(file.handle());// without this the tests would read only an empty file

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
    if(auto entry = this->hashes.find(relPath.toStdString()); entry != this->hashes.end()){
        if(entry->is_string()){
            std::string storedHash = *entry;
            bool matches = storedHash.c_str() == hash;
            this->eventListener.callOnFileProcessed(file, matches);
        }else{
            this->eventListener.callOnError(QStringLiteral("stored hash is not of type string; skipping"), file);
            this->eventListener.callOnFileProcessed(file, false);
        }
    }else{
        this->eventListener.callOnWarning(QStringLiteral("file has no saved hash; skipping"), file);
        this->eventListener.callOnFileProcessed(file, false);
    }
}

void LibTreeHashPrivate::updateEntry(const QString& file, const QString& relPath){
    QString hash = this->computeFileHash(file);
    if(hash.isNull()){
        this->eventListener.callOnFileProcessed(file, false);
        return;
    }

    this->hashes[relPath.toStdString()] = hash.toStdString();

    this->eventListener.callOnFileProcessed(file, true);
}

void LibTreeHashPrivate::openHashFile(QString& rootDir){
    QDir root(rootDir);
    if(!root.exists()){
        this->eventListener.callOnWarning(QStringLiteral("the root-dir dies not exist"), QStringLiteral("run"));
    }

    QString loadError;
    this->hashes = LibTreeHashPrivate::loadHashes(*this->hashFileSrc, &loadError);
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
                if(!this->hashes.contains(relPath.toStdString())){
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

json LibTreeHashPrivate::loadHashes(QFileDevice& hashFile, QString* error){
    try {
        std::string fileContent = hashFile.readAll().toStdString();
        if(fileContent.empty()){
            if(error != nullptr)
                *error = QString();
            return json::object();
        }
        json loaded = json::parse(fileContent);

        if(!loaded.is_object()){
            if(error != nullptr)
                *error = "unable to load hash-file: file is malformed (expected JSON-Object)";
            return json::object();
        }

        if(error != nullptr)
            *error = QString();
        return loaded;
    } catch (std::exception& e) {
        if(error != nullptr)
            *error = e.what();
        return json::object();
    }
}

bool LibTreeHashPrivate::ensureFileOpen(QFileDevice& file, bool write, QString* error){
    if(write){
        if(!file.isOpen()){
            if(!file.open(QFileDevice::OpenModeFlag::ExistingOnly | QFileDevice::OpenModeFlag::ReadWrite)){
                if(error != nullptr)
                    *error = QStringLiteral("file can not be opened (%1)").arg(file.errorString());
                return false;
            }
        }else{
            if(!file.isWritable()){
                if(error != nullptr)
                    *error = QStringLiteral("file is not writeable");
                return false;
            }
        }
    }else{
        if(!file.isOpen()){
            if(!file.open(QFileDevice::OpenModeFlag::ExistingOnly | QFileDevice::OpenModeFlag::ReadOnly)){
                if(error != nullptr)
                    *error = QStringLiteral("file can not be opened (%1)").arg(file.errorString());
                return false;
            }
        }else{
            if(!file.isReadable()){
                if(error != nullptr)
                    *error = QStringLiteral("file is not readable");
                return false;
            }
        }
    }

    return true;
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
        ret.append(QDir::cleanPath(fi.absoluteFilePath()));
    }

    return ret;
}

void TreeHash::cleanHashFile(const QString hashfilePath, const QString rootPath, QStringList keep, QString* error){
    QFile src(hashfilePath);
    QFile dst(hashfilePath);
    cleanHashFile(src, dst, rootPath, keep, error);
}

void TreeHash::cleanHashFile(QFileDevice& hashfileSrc, QFileDevice& hashfileDst, const QString rootPath,
                             QStringList keep, QString* error, bool truncDst){
    if(!LibTreeHashPrivate::ensureFileOpen(hashfileSrc, false, error))
        return;
    if(!LibTreeHashPrivate::ensureFileOpen(hashfileDst, true, error))
        return;

    QDir rootDir(rootPath);
    if(!rootDir.exists()){
        if(error != nullptr)
            *error = QStringLiteral("root does not exist");
        return;
    }

    QString loadError;
    json hashes = LibTreeHashPrivate::loadHashes(hashfileSrc, &loadError);
    if(!loadError.isNull()){
        if(error != nullptr)
            *error = loadError;
        return;
    }

    // filter hashes
    QSet<QString> keepSet(keep.begin(), keep.end());
    json filteredHashes;
    for(auto iter = hashes.begin(); iter != hashes.end(); iter++){
        QString f = QString::fromStdString(iter.key());
        QString absPath = rootDir.absoluteFilePath(f);
        if(keepSet.contains(f) || keepSet.contains(absPath)){
            filteredHashes[iter.key()] = iter.value();
        }
    }

    // save hashes
    std::string jsonData = filteredHashes.dump(4);

    if(truncDst){
        if(!hashfileDst.resize(0)){
            if(error != nullptr)
                *error = QStringLiteral("unable to save hashes: ") + hashfileDst.errorString();
            return;
        }
    }

    if(hashfileDst.write(jsonData.c_str()) == -1){
        if(error != nullptr)
            *error = QStringLiteral("unable to save hashes: ") + hashfileDst.errorString();
        return;
    }

    hashfileDst.flush();
    fsync(hashfileDst.handle());// without this the tests read only an empty file

    if(error != nullptr)
        *error = QString();
}

QStringList TreeHash::checkForRemovedFiles(const QString hashfilePath, const QString rootPath, const QStringList files, QString* error){
    QFile src(hashfilePath);
    return checkForRemovedFiles(src, rootPath, files, error);
}

QStringList TreeHash::checkForRemovedFiles(QFileDevice& hashfileSrc, const QString rootPath, const QStringList files, QString* error){
    QDir rootDir(rootPath);
    if(!rootDir.exists()){
        if(error != nullptr)
            *error = QStringLiteral("root does not exist");
        return QStringList();
    }

    if(!LibTreeHashPrivate::ensureFileOpen(hashfileSrc, false, error))
        return QStringList();

    QString loadError;
    json hashes = LibTreeHashPrivate::loadHashes(hashfileSrc, &loadError);
    if(!loadError.isNull()){
        if(error != nullptr)
            *error = loadError;
        return QStringList();
    }

    // find all removed files
    QStringList removed;
    for(auto iter = hashes.begin(); iter != hashes.end(); iter++){
        QString file = QString::fromStdString(iter.key());
        QString absPath = rootDir.absoluteFilePath(file);
        if(!files.contains(absPath)){
            removed.append(file);
        }
    }

    if(error != nullptr)
        *error = QString();
    return removed;
}
