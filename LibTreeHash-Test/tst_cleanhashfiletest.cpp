#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test creation of a new hash-file from tree
class CleanHashfileTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    CleanHashfileTest(){}
    ~CleanHashfileTest(){}

private slots:
    void initTestCase(){
        files.setup(false, true, false);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void cleanHashFile(){
        const QString hashfilePath = files.getD1FalseExpectedHashPath();
        const QString rootPath = files.getD1FalseData().path();

        QFile expectedJsonFile(hashfilePath);
        expectedJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject expectedJson = QJsonDocument::fromJson(expectedJsonFile.readAll()).object();

        QStringList keep;
        keep << "d1/d2/f3.dat" << (rootPath + "/d1/f2.dat");

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
            treeHash.setHashesFilePath(hashfilePath);
            treeHash.setRootDir(rootPath);
            treeHash.cleanHashFile(keep);
        } catch (...) {
            QVERIFY2(false, "treeHash threw exception");
        }

        // verify hashfile
        expectedJson.remove("d1/f1.dat");

        QFile actualJsonFile(hashfilePath);
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QString cmp = TestFiles::compareHashFiles(actualJson, expectedJson);
        QVERIFY2(cmp.isNull(),
                 QString("created hash-file did not contain the expected content (%1)").arg(cmp).toStdString().c_str());
    }
};

#include "tst_cleanhashfiletest.moc"
