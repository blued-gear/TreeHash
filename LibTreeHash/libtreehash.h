#ifndef LIBTREEHASH_H
#define LIBTREEHASH_H

#include <QString>
#include <QStringList>

namespace TreeHash{

enum class RunMode{
    /// updates the hashes of all files
    UPDATE,
    /// adds the hashes of new files
    UPDATE_NEW,
    /// checks all files against the stored hashes
    VERIFY
};

class EventListener{

    friend class LibTreeHash;
    friend class LibTreeHashPrivate;

public:

    /**
     * @brief called when a file was processed;
     *      ATTENTION: this method should not throw an exception
     * @param path the path of the processed file
     * @param success UPDATE: if no error occurred; VERIFY: if the hash matched, if any existed
     */
    std::function<void(QString, bool)> onFileProcessed;
    /**
     * @brief called when an anomaly occurred
     *      ATTENTION: this method should not throw an exception
     * @param msg a message
     * @param path location of anomaly (mostly a file path)
     */
    std::function<void(QString msg, QString path)> onWarning;
    /**
     * @brief called when an error occurred
     *      ATTENTION: this method should not throw an exception
     * @param msg a message
     * @param path location of error (mostly a file path)
     */
    std::function<void(QString msg, QString path)> onError;

private:

    void callOnFileProcessed(QString path, bool success){
        if(onFileProcessed)
            onFileProcessed(path, success);
    }
    void callOnWarning(QString msg, QString path){
        if(onWarning)
            onWarning(msg, path);
    }
    void callOnError(QString msg, QString path){
        if(onError)
            onError(msg, path);
    }

};

class LibTreeHashPrivate;
/**
 * @brief The LibTreeHash class provides the core functionality of the project,
 *      which is going through all files and creating or checking the hashes
 */
class LibTreeHash
{

    Q_DISABLE_COPY(LibTreeHash)

    LibTreeHashPrivate* priv;
    RunMode runMode = RunMode::VERIFY;
    QString rootDir = QStringLiteral("/");

public:
    LibTreeHash(const EventListener& listener);
    LibTreeHash(LibTreeHash&& mve);
    ~LibTreeHash();

    LibTreeHash& operator=(LibTreeHash&& mve);

    void run();

    /**
     * @brief sets the mode of operation.
     *      ATTENTION: do not change the mode while a process is running
     * @param mode the new mode
     */
    void setMode(RunMode mode){
        this->runMode = mode;
    }
    /**
     * @brief returns the current mode of execution
     */
    RunMode getRunMode() const{
        return this->runMode;
    }

    /**
     * @brief sets the HMAC-Key for the hash-function; if it is empty then HMAC is disabled.
     *      ATTENTION: do not change the value while a process is running
     * @param the HMAC-Key ("" to switch to normal hash)
     */
    void setHmacKey(QString hmac);

    QString getHmacKey() const;

    /**
     * @brief the root-dir is used do create relative paths for the file-entries in the hash-file
     *      ATTENTION: do not change the root-dir while a process is running
     * @param dir the root dir
     */
    void setRootDir(QString dir){
        this->rootDir = dir;
    }
    /**
     * @brief return the current root-dir
     */
    QString getRootDir() const{
        return this->rootDir;
    }

    /**
     * @brief sets the path of the file containing the hashes
     *      ATTENTION: do not change the path while a process is running
     * @param path the path to the hash-file
     */
    void setHashesFile(const QString path);

    /**
     * @brief returns the path of the currently used hash-file
     */
    const QString getHashesFile() const;

    /**
     * @brief sets all files to process
     *      ATTENTION: do not change the value while a process is running
     * @param paths list with all paths to all files to check
     */
    void setFiles(const QStringList paths);

    /**
     * @brief returns the paths of all files which will be (or are) processed
     */
    const QStringList getFiles() const;
};

/**
 * @brief lists all files recursively in the given root directory
 * @param root the root directory to start the search
 * @param includeLinkedDirs if true dir-symlinks will be followed
 * @param includeLinkedFiles if true file-symlinks will be included
 * @return a list with the absolute paths of all files in root
 */
QStringList listAllFilesInDir(const QString root, bool includeLinkedDirs, bool includeLinkedFiles);

/**
 * @brief removes all entries from the given hash-file which does not exist in keep
 * @param hashfilePath path to the hash-file
 * @param rootPath the path to the root-dir (to generate relative file-paths)
 * @param keep list with files to keep (either absolute paths or paths relative to root)
 * @param error if not nullptr error messages will be stored in error; if successful error will be a null-string
 */
void cleanHashFile(const QString hashfilePath, const QString rootPath, QStringList keep, QString* error = nullptr);

/**
 * @brief finds all files in which does not exist in the hash-file
 * @param hashfilePath path to the hash-file
 * @param rootPath the path to the root-dir (to generate relative file-paths)
 * @param files list of all files (absolute paths) to be counted as existing
 * @param error if not nullptr error messages will be stored in error; if successful error will be a null-string
 * @return a list with all paths (relative to rootPath) which did not occur in the hash-file
 */
QStringList checkForRemovedFiles(const QString hashfilePath, const QString rootPath, const QStringList files, QString* error = nullptr);
}

#endif // LIBTREEHASH_H
