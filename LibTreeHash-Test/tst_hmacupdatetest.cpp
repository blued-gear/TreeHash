#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test creation of a new hash-file from tree
class HmacUpdateTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    HmacUpdateTest(){}
    ~HmacUpdateTest(){}

private slots:
    void initTestCase(){
        files.setup(false, false, true);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void createHmacHashes(){
        const QString hashFileName("hmacHases.json");
        const QString hmacKey("a_Key");
        QDir dataDir = files.getD2Data();
        QDir hashFilesDir = files.getD2Hashes();

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

        try{
            treeHash.setMode(RunMode::UPDATE);
            treeHash.setRootDir(dataDir.path());
            treeHash.setHashesFilePath(hashFilesDir.filePath(hashFileName));
            treeHash.setFiles(paths);
            treeHash.setHmacKey(hmacKey);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }

        // verify created file
        QFile expectedJsonFile = files.getD2Expected();
        expectedJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject expectedJson = QJsonDocument::fromJson(expectedJsonFile.readAll()).object();

        QFile actualJsonFile(hashFilesDir.filePath(hashFileName));
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QVERIFY2(expectedJson == actualJson, "created hash-file did not contain the expected content");
    }

};

#include "tst_hmacupdatetest.moc"
