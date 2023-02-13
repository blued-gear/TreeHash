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

        QString err("_");
        TreeHash::cleanHashFile(hashfilePath, rootPath, keep, &err);
        QVERIFY(err.isNull());

        // verify hashfile
        expectedJson.remove("d1/f1.dat");

        QFile actualJsonFile(hashfilePath);
        actualJsonFile.open(QFile::OpenModeFlag::ReadOnly);
        QJsonObject actualJson = QJsonDocument::fromJson(actualJsonFile.readAll()).object();

        QVERIFY2(expectedJson == actualJson, "created hash-file did not contain the expected content");
    }
};

#include "tst_cleanhashfiletest.moc"
