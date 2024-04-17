#include <QCoreApplication>
#include <QCommandLineParser>
#include <iostream>
#include <QDir>
#include <QFile>
#include <QMetaEnum>
#include "libtreehash.h"

/* exit codes:
 * 0 => success
 * -1 => invalid arguments (also invalid paths)
 * -2 => error from LibTreeHash
 * 1 => >= 1 file was unsuccessful
 * 2 => if an error occurred
 */

namespace{
QStringList listFiles(QCommandLineParser& args){
    const QDir root(args.value("r"));
    QFileInfo fi;
    QStringList files;

    const bool includeLinkedDirs = !args.isSet("no-linked-dirs");
    const bool includeLinkedFiles = !args.isSet("no-linked-files");

    // 1. use included dirs or root
    if(args.isSet("i")){
        // use include
        for(const QString& i : args.values("i")){
            QString path = root.absoluteFilePath(i);
            fi.setFile(path);

            if(fi.isDir()){
                files.append(TreeHash::listAllFilesInDir(fi.absoluteFilePath(), includeLinkedDirs, includeLinkedFiles));
            }
        }
    }else{
        // scan root
        files = TreeHash::listAllFilesInDir(root.path(), includeLinkedDirs, includeLinkedFiles);
    }

    files.removeDuplicates();

    // 2. remove excluded dirs and files
    for(const QString& e : args.values("e")){
        QString path = QDir::cleanPath(root.absoluteFilePath(e));
        fi.setFile(path);

        if(fi.isDir()){
            // remove subtree
            files.removeIf([path](const QString& entry) -> bool{
                return entry.startsWith(path);
            });
        }else if(fi.isFile()){
            files.removeOne(path);
        }else{
            std::cerr << QStringLiteral("invalid exclude-path (ignoring): %1\n").arg(e).toStdString();
        }
    }

    // 3. include explicit included files
    if(args.isSet("i")){
        for(const QString& i : args.values("i")){
            QString path = QDir::cleanPath(root.absoluteFilePath(i));
            fi.setFile(path);

            if(fi.isDir()){
                continue;
            }else if(fi.isFile()){
                files.append(path);
            }else{
                std::cerr << QStringLiteral("invalid include-path (ignoring): %1\n").arg(i).toStdString();
            }
        }
    }

    // 4. remove hashfile
    QFileInfo hashfileInfo(args.value("f"));
    files.removeOne(QDir::cleanPath(hashfileInfo.absoluteFilePath()));

    return files;
}

bool initLibTreeHash(QCommandLineParser& args, TreeHash::LibTreeHash& treeHash, int& exitCode, bool needsMode){
    if(args.values("r").size() > 1){
        std::cerr << "root must be set no more than once\n";
        exitCode = -1;
        return false;
    }
    if(args.values("f").size() != 1){
        std::cerr << "hashfile must be set exactly once\n";
        exitCode = -1;
        return false;
    }
    if(args.values("m").size() != 1 && needsMode){
        std::cerr << "mode must be set exactly once\n";
        exitCode = -1;
        return false;
    }
    if(args.values("l").size() > 1){
        std::cerr << "loglevel must not be set more than once\n";
        exitCode = -1;
        return false;
    }
    if(args.values("k").size() > 1){
        std::cerr << "hamc-key must not be set more than once\n";
        exitCode = -1;
        return false;
    }

    const QString loglevelStr = args.value("l");
    int loglevel;
    if(loglevelStr == "q"){
        loglevel = 0;
    }else if(loglevelStr == "e"){
        loglevel = 1;
    }else if(loglevelStr == "w"){
        loglevel = 2;
    }else if(loglevelStr == "a"){
        loglevel = 3;
    }else if(loglevelStr.isEmpty()){
        // use default value
        loglevel = 2;
    }else{
        std::cerr << "loglevel has an invalid value\n\n";
        args.showHelp(-1);
        exitCode = -1;
        return false;
    }

    bool hashfileFromStdin = args.value("f") == "-";

    TreeHash::EventListener eventListener;
    eventListener.onFileProcessed = [loglevel, &exitCode, hashfileFromStdin](QString path, bool success) -> void{
        if(!success){
            if(loglevel >= 1){
                if(hashfileFromStdin)
                    std::cerr << QStringLiteral("file unsuccessful: %1\n").arg(path).toStdString();
                else
                    std::cout << QStringLiteral("file unsuccessful: %1\n").arg(path).toStdString();
            }

            if(exitCode < 1)
                exitCode = 1;
        }else{
            if(loglevel >= 3){
                if(hashfileFromStdin)
                    std::cerr << QStringLiteral("file successful: %1\n").arg(path).toStdString();
                else
                    std::cout << QStringLiteral("file successful: %1\n").arg(path).toStdString();
            }
        }
    };
    eventListener.onWarning = [loglevel, hashfileFromStdin](QString msg, QString path) -> void{
        if(loglevel >= 2){
            if(hashfileFromStdin)
                std::cerr << QStringLiteral("WARNING: %1 @ %2\n").arg(msg, path).toStdString();
            else
                std::cout << QStringLiteral("WARNING: %1 @ %2\n").arg(msg, path).toStdString();
        }
    };
    eventListener.onError = [loglevel, &exitCode, hashfileFromStdin](QString msg, QString path) -> void{
        if(loglevel >= 1){
            if(hashfileFromStdin)
                std::cerr << QStringLiteral("ERROR: %1 @ %2\n").arg(msg, path).toStdString();
            else
                std::cout << QStringLiteral("ERROR: %1 @ %2\n").arg(msg, path).toStdString();
        }

        if(exitCode < 2)
            exitCode = 2;
    };

    treeHash = TreeHash::LibTreeHash(eventListener);

    if(needsMode){
        const QString modeStr = args.value("m");
        TreeHash::RunMode mode;
        if(modeStr == "update"){
            mode = TreeHash::RunMode::UPDATE;
        }else if(modeStr == "update_new"){
            mode = TreeHash::RunMode::UPDATE_NEW;
        }else if(modeStr == "update_mod"){
            mode = TreeHash::RunMode::UPDATE_MODIFIED;
        }else if(modeStr == "verify"){
            mode = TreeHash::RunMode::VERIFY;
        }else{
            std::cerr << "mode has an invalid value\n\n";
            args.showHelp(-1);
            exitCode = -1;
            return false;
        }

        treeHash.setMode(mode);
    }

    if(args.isSet("hash-alg")){
        const QString hashAlgStr = args.value("hash-alg");
        bool valid;
        auto algoVal = QMetaEnum::fromType<QCryptographicHash::Algorithm>().keyToValue(hashAlgStr.toStdString().c_str(), &valid);
        if(valid){
            QCryptographicHash::Algorithm hashAlg = static_cast<QCryptographicHash::Algorithm>(algoVal);
            treeHash.setHashAlgorithm(hashAlg);
        }else{
            std::cerr << "invalid hash-algorithm\n";
            exitCode = -1;
            return false;
        }
    }

    if(!hashfileFromStdin){
        QFileInfo hashfileInfo(args.value("f"));
        if(hashfileInfo.exists()){
            if(!hashfileInfo.isFile()){
                std::cerr << "hash-file is not a file\n";
                exitCode = -1;
                return false;
            }
        }else{
            if(treeHash.getRunMode() == TreeHash::RunMode::VERIFY){
                std::cerr << "hash-file does not exist\n";
                exitCode = -1;
                return false;
            }
        }
    }

    treeHash.setFiles(listFiles(args));
    treeHash.setHmacKey(args.value("k"));

    if(hashfileFromStdin){
        auto in = std::make_unique<QFile>();
        auto out = std::make_unique<QFile>();

        if(!in->open(stdin, QFile::OpenModeFlag::ReadOnly, QFile::FileHandleFlag::DontCloseHandle)){
            std::cerr << QStringLiteral("unable to open stdin (%1)\n").arg(in->errorString()).toStdString();
            exitCode = -2;
            return false;
        }
        if(!out->open(stdout, QFile::OpenModeFlag::WriteOnly, QFile::FileHandleFlag::DontCloseHandle)){
            std::cerr << QStringLiteral("unable to open stdout (%1)\n").arg(out->errorString()).toStdString();
            exitCode = -2;
            return false;
        }

        treeHash.setHashesFile(std::move(in), std::move(out), false);
    }else{
        treeHash.setHashesFilePath(args.value("f"));
    }

    if(args.isSet("r")){
        if(!QFileInfo(args.value("r")).isDir()){
            std::cerr << "root does not exist or is not a directory\n";
            exitCode = -1;
            return false;
        }
        treeHash.setRootDir(args.value("r"));
    }else{
        if(treeHash.getRootDir().isNull()){
            std::cerr << "root was not provided and was not stored in hashfile\n";
            exitCode = -1;
            return false;
        }
    }

    exitCode = 0;
    return true;
}

void setupCommands(QCommandLineParser& parser){
    parser.setSingleDashWordOptionMode(QCommandLineParser::SingleDashWordOptionMode::ParseAsCompactedShortOptions);
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::OptionsAfterPositionalArgumentsMode::ParseAsOptions);

    parser.addOptions({
        {{"m", "mode"},
            "mode of operation",
            "'update', 'update_new', 'update_mod' or 'verify'"},
        {{"l", "loglevel"},
            "sets the verbosity of the log ('w' is default)",
            "'q' -> print nothing, 'e' -> show only errors, 'w' -> show errors and warnings, 'a' -> show errors, warnings and processed files"},
        {{"r", "root"},
            "sets the root-directory (for listing files)",
            "root-path"},
        {{"f", "hashfile"},
            "path to the hash-file (file which will contain the generated hashes)",
            "file-path; if set to '-' the data will be read from stdin and (if in update-mode) be written to stdout "
                "(all log-messages will be written to stderr)"},
        {{"e", "exclude"},
            "path to file or directory to exclude from hashing (relative to --root)",
            "path to exclude"},
        {{"i", "include"},
            "path to file or directory to include for hashing (relative to --root); if this option is set, then only the specified files will be used",
            "path to include"},
        {{"k", "hmac-key"},
            "if set, the hashes will be computed as HMACs with the provided key",
            "key"},
        {{"c", "clean"},
            "cleans the hash-file: removes all files which does not exist any-more (can be used with -e and -i) (it might be smart to make a backup of the file)"},
        {"check-removed",
            "checks if any files from the hashfile does not exist any-more (can be used with -e and -i)"},
        {"no-linked-dirs",
            "exclude linked directories from scan"},
        {"no-linked-files",
            "exclude linked files from scan"},
        {"hash-alg",
            "set the algorithm to use for computing the hashes",
            "Sha256, Sha512, Sha3_256, Sha3_512, Keccak_256, Keccak_512 (default), Blake2b_256, Blake2b_512"}
    });

    parser.addHelpOption();
    parser.addVersionOption();
}

int execNormal(QCommandLineParser& args){
    try{
        TreeHash::LibTreeHash treeHash;
        int exitCode = 0;

        if(initLibTreeHash(args, treeHash, exitCode, true)){
            treeHash.run();
        }

        return exitCode;
    }catch(std::exception& e){
        std::cerr << "LibTreeHash threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int execClean(QCommandLineParser& args){
    try{
        TreeHash::LibTreeHash treeHash;
        int exitCode = 0;

        if(initLibTreeHash(args, treeHash, exitCode, false)){
            QStringList keep = listFiles(args);
            treeHash.cleanHashFile(keep);
        }

        return exitCode;
    }catch(std::exception& e){
        std::cerr << "LibTreeHash threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int execRemoved(QCommandLineParser& args){
    try{
        TreeHash::LibTreeHash treeHash;
        int exitCode = 0;

        if(initLibTreeHash(args, treeHash, exitCode, false)){
            QStringList existing = listFiles(args);
            QStringList missing = treeHash.checkForRemovedFiles(existing);

            if(exitCode == 0){// errors would change exitCode in eventListener
                // remove all excluded files from missing
                QDir root(treeHash.getRootDir());
                QFileInfo fi;
                for(const QString& e : args.values("e")){
                    QString path = root.absoluteFilePath(e);
                    QString relPath = root.relativeFilePath(e);
                    fi.setFile(path);

                    if(fi.isDir()){
                        // remove subtree
                        missing.removeIf([relPath](const QString& entry) -> bool{
                            return entry.startsWith(relPath);
                        });
                    }else if(fi.isFile()){
                        missing.removeOne(path);
                    }else if(!fi.exists()){
                        // missing paths also can be excluded -> QFileInfo can not determine type -> use heuristic: if paths ends with '/' it is treated as a dir
                        if(e.endsWith("/")){
                            // remove subtree
                            missing.removeIf([relPath](const QString& entry) -> bool{
                                return entry.startsWith(relPath);
                            });
                        }else{
                            missing.removeOne(path);
                        }
                    }
                }

                for(const QString& line : missing)
                    std::cout << line.toStdString() << '\n';
            }
        }

        return exitCode;
    }catch(std::exception& e){
        std::cerr << "LibTreeHash threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int exec(QCommandLineParser& parser){
    // check for mandatory options
    if(!parser.isSet("f")){
        std::cerr << "option -f (hashfile) is mandatory\n\n";
        parser.showHelp(-1);
        return -1;
    }

    // detect mode (clean or normal) and execute
    if(parser.isSet("c")){
        // check for incompatible options
        if(parser.isSet("m") || parser.isSet("k") || parser.isSet("check-removed")){
            std::cerr << "-c cannot be used with -m, -k or --check-removed\n";
            return -1;
        }

        return execClean(parser);
    }else if(parser.isSet("check-removed")){
        // check for incompatible options
        if(parser.isSet("m") || parser.isSet("k") || parser.isSet("c")){
            std::cerr << "-c cannot be used with -m, -k or -c\n";
            return -1;
        }

        return execRemoved(parser);
    }else{
        return execNormal(parser);
    }
}
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    a.setApplicationName("TreeHash");
    a.setApplicationVersion("1.0");

    QCommandLineParser cliParser;
    cliParser.setApplicationDescription("TreeHash is an utility to create and verify hashes for a file-tree.");
    setupCommands(cliParser);

    cliParser.process(a);

    return exec(cliParser);
}
