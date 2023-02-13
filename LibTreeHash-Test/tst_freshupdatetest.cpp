#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>

#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test creation of a new hash-file from tree
class FreshUpdateTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    FreshUpdateTest(){}
    ~FreshUpdateTest(){}

private slots:
    void initTestCase(){
        files.setup(true, false, false);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void listAllFiles(){
        QDir dataDir = files.getD1Data();
        QStringList paths = listAllFilesInDir(dataDir.path(), false, false);
        QCOMPARE(paths.size(), 3);
        QString root = dataDir.absolutePath();
        QVERIFY(paths.contains(root + "/d1/f1.dat"));
        QVERIFY(paths.contains(root + "/d1/f2.dat"));
        QVERIFY(paths.contains(root + "/d1/d2/f3.dat"));
    }

    void createFullHashes(){
        const QString hashFileName("createFullHashes.json");
        QDir dataDir = files.getD1Data();
        QDir hashFilesDir = files.getD1Hashes();

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
            treeHash.setHashAlgorithm(QCryptographicHash::Algorithm::Blake2b_256);
            treeHash.setHashesFilePath(hashFilesDir.filePath(hashFileName));
            treeHash.setFiles(paths);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }

        // verify created file
        QFile expectedJsonFile = files.getD1ExpectedHashFile();
        expectedJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject expectedJson = QJsonDocument::fromJson(expectedJsonFile.readAll()).object();

        QFile actualJsonFile(hashFilesDir.path() + "/" + hashFileName);
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QString cmp = TestFiles::compareHashFiles(actualJson, expectedJson);
        QVERIFY2(cmp.isNull(),
                 QString("created hash-file did not contain the expected content (%1)").arg(cmp).toStdString().c_str());
    }

};

#include "tst_freshupdatetest.moc"
