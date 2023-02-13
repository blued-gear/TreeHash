#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include "testfiles.h"
#include "libtreehash.h"

using namespace TreeHash;

/// test creation of a new hash-file from tree
class VerifyTest : public QObject
{
    Q_OBJECT

private:
    TestFiles files;

public:
    VerifyTest(){}
    ~VerifyTest(){}

private slots:
    void initTestCase(){
        files.setup(true, true, false);
    }

    void cleanupTestCase(){
        files.cleanup();
    }

    void verify(){
        QDir dataDir = files.getD1Data();
        QString verifyFilePath = files.getD1ExpectedFilePath();

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
            treeHash.setMode(RunMode::VERIFY);
            treeHash.setRootDir(dataDir.path());
            treeHash.setHashAlgorithm(QCryptographicHash::Algorithm::Blake2b_256);
            treeHash.setHashesFilePath(verifyFilePath);
            treeHash.setFiles(paths);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }
    }

    void verifyFalse(){
        QDir dataDir = files.getD1FalseData();
        QString verifyFilePath = files.getD1FalseHashFilePath();

        bool encounteredF1 = false, encounteredF2 = false, encounteredF3 = false;
        EventListener listener;
        listener.onError = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported error: " + msg).toStdString().c_str());
        };
        listener.onWarning = [](QString msg, QString path) -> void{
            QVERIFY2(false, ("treeHash reported warning: " + msg).toStdString().c_str());
        };
        listener.onFileProcessed = [&encounteredF1, &encounteredF2, &encounteredF3](QString path, bool success) -> void{
            if(path.endsWith("d1/d2/f3.dat")){
                QVERIFY2(encounteredF3 == false, "f3 encountered multiple times");
                QVERIFY2(success == true, "expected success");
                encounteredF3 = true;
            }else if(path.endsWith("d1/f1.dat")){
                QVERIFY2(encounteredF1 == false, "f1 encountered multiple times");
                QVERIFY2(success == false, "expected failure");
                encounteredF1 = true;
            }else if(path.endsWith("d1/f2.dat")){
                QVERIFY2(encounteredF2 == false, "f2 encountered multiple times");
                QVERIFY2(success == true, "expected success");
                encounteredF2 = true;
            }else{
                QVERIFY2(false, ("unexpected file: " + path).toStdString().c_str());
            }
        };

        LibTreeHash treeHash(listener);

        QStringList paths = listAllFilesInDir(dataDir.path(), false, false);

        try{
            treeHash.setMode(RunMode::VERIFY);
            treeHash.setRootDir(dataDir.path());
            treeHash.setHashesFilePath(verifyFilePath);
            treeHash.setFiles(paths);

            treeHash.run();
        }catch(...){
            QVERIFY2(false, "treeHash threw exception");
        }
    }
};

#include "tst_verifytest.moc"
