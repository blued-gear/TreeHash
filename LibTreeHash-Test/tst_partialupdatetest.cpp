#include <QtTest>

#include <QJsonDocument>
#include <QRegularExpression>

#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test creation of a new hash-file from tree
class PartialUpdateTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    PartialUpdateTest(){}
    ~PartialUpdateTest(){}

private slots:
    void initTestCase(){
        files.setup(false, true);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void createPartialHashes(){
        updateHashes();
        verifyHashes();
    }

private:
    void updateHashes(){
        QDir dataDir = files.getD1FalseData();
        const QString hashFile = files.getD1FalseHashFilePath();

        EventListener listener;
        listener.onError = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported error: " + msg).toStdString().c_str());
        };
        listener.onWarning = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported warning: " + msg).toStdString().c_str());
        };
        listener.onFileProcessed = [](QString path, bool success) -> void{
            QVERIFY2(success, ("treeHash reported could not process file: " + path).toStdString().c_str());
        };

        LibTreeHash treeHash(listener);

        QStringList paths = listAllFilesInDir(dataDir.path(), false, false);
        paths = paths.filter(QRegularExpression(".+\\/d1\\/f1\\.dat|.*\\/d1\\/f2\\.dat"));// just use f1 and f2

        try{
            treeHash.setMode(RunMode::UPDATE);
            treeHash.setRootDir(dataDir.path());
            treeHash.setHashesFile(hashFile);
            treeHash.setFiles(paths);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }
    }

    void verifyHashes(){
        QDir dataDir = files.getD1FalseData();
        const QString hashFile = files.getD1FalseHashFilePath();
        const QString expectedHashFile = files.getD1FalseExpectedHashPath();

        // verify created file
        QFile expectedJsonFile(expectedHashFile);
        expectedJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject expectedJson = QJsonDocument::fromJson(expectedJsonFile.readAll()).object();

        QFile actualJsonFile(hashFile);
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QVERIFY2(expectedJson == actualJson, "created hash-file did not contain the expected content");
    }
};

#include "tst_partialupdatetest.moc"
