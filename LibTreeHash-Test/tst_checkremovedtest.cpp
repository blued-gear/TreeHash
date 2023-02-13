#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

#include "testfiles.h"
#include "libtreehash.h"

/// test creation of a new hash-file from tree
class CheckRemovedTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    CheckRemovedTest(){}
    ~CheckRemovedTest(){}

private slots:
    void initTestCase(){
        files.setup(true, false, false);

        // delete a subdir
        QDir del(files.getD1Data().absoluteFilePath("d1/d2"));
        QVERIFY(del.removeRecursively());
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void checkForRemovedFiles(){
        QString root = files.getD1Data().path();
        QString hashfile = files.getD1ExpectedFilePath();

        QStringList existing = TreeHash::listAllFilesInDir(root, false, false);

        TreeHash::EventListener listener;
        listener.onError = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported error: " + msg).toStdString().c_str());
        };
        listener.onWarning = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported warning: " + msg).toStdString().c_str());
        };
        listener.onFileProcessed = [](QString path, bool success) -> void{
            QVERIFY2(false, ("treeHash reported a process file: " + path).toStdString().c_str());
        };

        TreeHash::LibTreeHash treeHash(listener);

        try {
            treeHash.setHashesFilePath(hashfile);
            treeHash.setRootDir(root);
            QStringList missing = treeHash.checkForRemovedFiles(existing);

            QVERIFY(missing.size() == 1);
            QVERIFY(missing.at(0) == "d1/d2/f3.dat");
        } catch (...) {
            QVERIFY2(false, "treeHash threw exception");
        }
    }

};

#include "tst_checkremovedtest.moc"
