#ifndef LIBTREEHASH_H
#define LIBTREEHASH_H

#include <QString>
#include <QStringList>

namespace TreeHash{

enum class RunMode{
    /// updates the hashes of all files
    UPDATE,
    /// checks all files against the stored hashes
    VERIFY
};

class EventListener{

    friend class LibTreeHash;

public:

    /**
     * @brief called when a file was processed;
     *      ATTENTION: this method should not throw an exception
     * @param path the path of the processed file
     * @param success UPDATE: if no error occurred; VERIFY: if the hash matched, if any existed
     */
    void (*onFileProcessed)(QString path, bool success) = nullptr;
    /**
     * @brief called when an anomaly occurred
     *      ATTENTION: this method should not throw an exception
     * @param msg a message
     * @param path location of anomaly (mostly a file path)
     */
    void (*onWarning)(QString msg, QString path) = nullptr;
    /**
     * @brief called when an error occurred
     *      ATTENTION: this method should not throw an exception
     * @param msg a message
     * @param path location of error (mostly a file path)
     */
    void (*onError)(QString msg, QString path) = nullptr;

private:

    void callOnFileProcessed(QString path, bool success){
        if(onFileProcessed != nullptr)
            (*onFileProcessed)(path, success);
    }
    void callOnWarning(QString msg, QString path){
        if(onWarning != nullptr)
            (*onWarning)(msg, path);
    }
    void callOnError(QString msg, QString path){
        if(onError != nullptr)
            (*onError)(msg, path);
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

public:
    LibTreeHash(const EventListener& listener);
    LibTreeHash(LibTreeHash&& mve);
    ~LibTreeHash();

    LibTreeHash& operator=(LibTreeHash&& mve);

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
    RunMode getRunMode(){
        return this->runMode;
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
    const QString getHashesFile();

    /**
     * @brief sets all files to process
     *      ATTENTION: do not change the value while a process is running
     * @param paths list with all paths to all files to check
     */
    void setFiles(const QStringList paths);

    /**
     * @brief returns the paths of all files which will be (or are) processed
     */
    const QStringList getFiles();
};

/**
 * @brief lists all files recursively in the given root directory
 * @param root the root directory to start the search
 * @param includeLinkedDirs if true dir-symlinks will be followed
 * @param includeLinkedFiles if true file-symlinks will be included
 * @return a list with the paths of all files in root
 */
QStringList listAllFilesInDir(const QString root, bool includeLinkedDirs, bool includeLinkedFiles);
}

#endif // LIBTREEHASH_H
