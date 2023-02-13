#ifndef TESTFILES_H
#define TESTFILES_H

#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "quazip/quazip.h"
#include "quazip/quazipfile.h"

class TestFiles{

    QTemporaryDir dirPath;
    QDir dir;

    QDir d1, d1_data, d1_hashes;
    QString d1_expected;

    QDir d1False, d1False_data, d1False_hashes;
    QString d1False_hashfile, d1False_hashfile2, d1False_hashfile3;

    QDir d2, d2_data, d2_hashes;
    QString d2_expected;

public:
    void setup(bool d1, bool d1False, bool d2){
        Q_ASSERT(dirPath.isValid());
        dir.setPath(dirPath.path());
        printf("testDir: %s\n", dir.path().toStdString().c_str());


        if(d1){
            extractD1();

            d1_expected = d1_hashes.path() + "/d1-verify.json";
            extractResFile(":testfiles/d1-expected.json", d1_expected);
        }

        if(d1False)
            extractD1False();

        if(d2){
            extractD2();

            d2_expected = ":testfiles/d2-expected.json";
        }
    }

    void cleanup(){
        dirPath.remove();

        QString emptyPath;
        d1.setPath(emptyPath);
        d1_data.setPath(emptyPath);
        d1_hashes.setPath(emptyPath);
        d1_expected = emptyPath;
        d1False.setPath(emptyPath);
        d1False_data.setPath(emptyPath);
        d1False_hashes.setPath(emptyPath);
        d1False_hashfile = emptyPath;
        d1False_hashfile2 = emptyPath;
        d1False_hashfile3 = emptyPath;
        d2.setPath(emptyPath);
        d2_data.setPath(emptyPath);
        d2_hashes.setPath(emptyPath);
        d2_expected = emptyPath;
    }

    QDir getD1Data(){
        return d1_data;
    }
    QDir getD1Hashes(){
        return d1_hashes;
    }
    QFile getD1ExpectedHashFile(){
        return QFile(d1_expected);
    }
    QString getD1ExpectedFilePath(){
        return d1_expected;
    }

    QDir getD1FalseData(){
        return d1False_data;
    }
    QString getD1FalseHashFilePath(){
        return d1False_hashfile;
    }
    QString getD1FalseExpectedHashPath(){
        return d1False_hashfile2;
    }
    QString getD1FalseMissingHashPath(){
        return d1False_hashfile3;
    }

    QDir getD2Data(){
        return d2_data;
    }
    QDir getD2Hashes(){
        return d2_hashes;
    }
    QFile getD2Expected(){
        return QFile(d2_expected);
    }

private:
    void extractD1(){
        // create dirs
        d1.setPath(dir.path() + "/d1");
        Q_ASSERT(dir.mkdir("d1"));
        d1_data = QDir(d1.path() + "/tree");
        Q_ASSERT(d1.mkdir("tree"));
        d1_hashes = QDir(d1.path() + "/hashes");
        Q_ASSERT(d1.mkdir("hashes"));

        extractZip(":/testfiles/d1.zip", d1_data);
    }

    void extractD1False(){
        // create dirs
        d1False.setPath(dir.path() + "/d1False");
        Q_ASSERT(dir.mkdir("d1False"));
        d1False_data = QDir(d1False.path() + "/tree");
        d1False_hashes = QDir(d1False.path() + "/hashes");

        extractZip(":/testfiles/d1-false.zip", d1False);
        Q_ASSERT(d1False_data.exists());
        Q_ASSERT(d1False_hashes.exists());

        d1False_hashfile = d1False_hashes.path() + "/hashes.json";
        d1False_hashfile2 = d1False_hashes.path() + "/expected.json";
        d1False_hashfile3 = d1False_hashes.path() + "/hashes-missing.json";
    }

    void extractD2(){
        // create dirs
        d2.setPath(dir.path() + "/d2");
        Q_ASSERT(dir.mkdir("d2"));
        d2_data = QDir(d2.path() + "/tree");
        Q_ASSERT(d2.mkdir("tree"));
        d2_hashes = QDir(d2.path() + "/hashes");
        Q_ASSERT(d2.mkdir("hashes"));

        extractZip(":/testfiles/d2.zip", d2_data);
    }

    void extractZip(QString src, QDir dest){
        QuaZip testData(src);
        Q_ASSERT(testData.open(QuaZip::Mode::mdUnzip));
        QFile out;
        for(bool hasNext = testData.goToFirstFile(); hasNext; hasNext = testData.goToNextFile()){
            QuaZipFileInfo zipfileEntry;
            Q_ASSERT(testData.getCurrentFileInfo(&zipfileEntry));

            if(!zipfileEntry.name.contains(".")){
                // create subdir
                Q_ASSERT(dest.mkdir(zipfileEntry.name));
            }else{
                // extract file
                out.setFileName(dest.path() + "/" + zipfileEntry.name);
                Q_ASSERT(out.open(QFile::OpenModeFlag::WriteOnly));
                QuaZipFile zipfile(&testData);
                Q_ASSERT(zipfile.open(QFile::OpenModeFlag::ReadOnly));

                QByteArray zipfileContent = zipfile.readAll();
                out.write(zipfileContent);

                out.close();
            }
        }
    }

    void extractResFile(QString src, QString dest){
        QFile srcFile(src);
        srcFile.open(QFile::OpenModeFlag::ReadOnly);
        QFile destFile(dest);
        destFile.open(QFile::OpenModeFlag::WriteOnly);
        destFile.write(srcFile.readAll());
        destFile.flush();
    }
};

#endif // TESTFILES_H
