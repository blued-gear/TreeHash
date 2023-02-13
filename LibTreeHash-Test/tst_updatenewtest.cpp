#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test update of a hash-file with only changed files
class UpdateNewTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    UpdateNewTest(){}
    ~UpdateNewTest(){}

private slots:
    void initTestCase(){
        files.setup(false, true, false);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void createFullHashes(){
        QDir dataDir = files.getD1FalseData();
        QString hashFile = files.getD1FalseMissingHashPath();

        EventListener listener;
        listener.onError = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported error: " + msg).toStdString().c_str());
        };
        listener.onWarning = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported warning: " + msg).toStdString().c_str());
        };
        listener.onFileProcessed = [](QString path, bool success) -> void{
            QVERIFY2(success, ("treeHash reported could not process file: " + path).toStdString().c_str());

            QVERIFY2(path.endsWith("d1/f1.dat"), ("unexpected file was processed: " + path).toStdString().c_str());
        };

        LibTreeHash treeHash(listener);

        QStringList paths = listAllFilesInDir(dataDir.path(), false, false);

        try{
            treeHash.setMode(RunMode::UPDATE_NEW);
            treeHash.setRootDir(dataDir.path());
            treeHash.setHashesFile(hashFile);
            treeHash.setFiles(paths);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }

        // verify created file
        QFile expectedJsonFile(files.getD1FalseExpectedHashPath());
        expectedJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject expectedJson = QJsonDocument::fromJson(expectedJsonFile.readAll()).object();

        QFile actualJsonFile(hashFile);
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QVERIFY2(expectedJson == actualJson, "created hash-file did not contain the expected content");
    }

};

#include "tst_updatenewtest.moc"
