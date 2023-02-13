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
        Q_ASSERT(del.removeRecursively());
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void checkForRemovedFiles(){
        QString root = files.getD1Data().path();
        QString hashfile = files.getD1ExpectedFilePath();

        QStringList existing = TreeHash::listAllFilesInDir(root, false, false);

        QString err("_");
        QStringList missing = TreeHash::checkForRemovedFiles(hashfile, root, existing, &err);

        QVERIFY(err.isNull());
        QVERIFY(missing.size() == 1);
        QVERIFY(missing.at(0) == "d1/d2/f3.dat");
    }

};

#include "tst_checkremovedtest.moc"
