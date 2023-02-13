#include <QCoreApplication>
#include <QCommandLineParser>
#include <iostream>
#include <QDir>
#include <QFile>
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

void setupCommands(QCommandLineParser& parser){
    parser.setSingleDashWordOptionMode(QCommandLineParser::SingleDashWordOptionMode::ParseAsCompactedShortOptions);
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::OptionsAfterPositionalArgumentsMode::ParseAsOptions);

    parser.addOptions({
        {{"m", "mode"},
            "mode of operation",
            "'update', 'update_new' or 'verify'"},
        {{"l", "loglevel"},
            "sets the verbosity of the log ('w' is default)",
            "'q' -> print nothing, 'e' -> show only errors, 'w' -> show errors and warnings, 'a' -> show erroes, warnings and processed files"},
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
            "path to exclude"},
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
    if(args.values("r").size() != 1){
        std::cerr << "root must be set exactly once\n";
        return -1;
    }
    if(args.values("f").size() != 1){
        std::cerr << "hashfile must be set exactly once\n";
        return -1;
    }
    if(args.values("m").size() != 1){
        std::cerr << "mode must be set exactly once\n";
        return -1;
    }
    if(args.values("l").size() > 1){
        std::cerr << "loglevel must not be set more than once\n";
        return -1;
    }
    if(args.values("k").size() > 1){
        std::cerr << "hamc-key must not be set more than once\n";
        return -1;
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
        return -1;
    }

    const QString modeStr = args.value("m");
    TreeHash::RunMode mode;
    if(modeStr == "update"){
        mode = TreeHash::RunMode::UPDATE;
    }else if(modeStr == "update_new"){
        mode = TreeHash::RunMode::UPDATE_NEW;
    }else if(modeStr == "verify"){
        mode = TreeHash::RunMode::VERIFY;
    }else{
        std::cerr << "mode has an invalid value\n\n";
        args.showHelp(-1);
        return -1;
    }

    QCryptographicHash::Algorithm hashAlg = QCryptographicHash::Algorithm::Keccak_512;
    if(args.isSet("hash-alg")){
        const QString hashAlgStr = args.value("hash-alg");
        if(hashAlgStr == "Sha256")
            hashAlg = QCryptographicHash::Algorithm::Sha256;
        else if(hashAlgStr == "Sha512")
            hashAlg = QCryptographicHash::Algorithm::Sha512;
        else if(hashAlgStr == "Sha3_256")
            hashAlg = QCryptographicHash::Algorithm::Sha3_256;
        else if(hashAlgStr == "Sha3_512")
            hashAlg = QCryptographicHash::Algorithm::Sha3_512;
        else if(hashAlgStr == "Keccak_256")
            hashAlg = QCryptographicHash::Algorithm::Keccak_256;
        else if(hashAlgStr == "Keccak_512")
            hashAlg = QCryptographicHash::Algorithm::Keccak_512;
        else if(hashAlgStr == "Blake2b_256")
            hashAlg = QCryptographicHash::Algorithm::Blake2b_256;
        else if(hashAlgStr == "Blake2b_512")
            hashAlg = QCryptographicHash::Algorithm::Blake2b_512;
        else{
            std::cerr << "invalid hash-algorithm\n";
            return -1;
        }
    }

    if(!QFileInfo(args.value("r")).isDir()){
        std::cerr << "root does not exist or is not a directory\n";
        return -1;
    }

    bool hashfileFromStdin = args.value("f") == "-";

    if(!hashfileFromStdin){
        QFileInfo hashfileInfo(args.value("f"));
        if(hashfileInfo.exists()){
            if(!hashfileInfo.isFile()){
                std::cerr << "hash-file is not a file\n";
                return -1;
            }
        }else{
            if(mode == TreeHash::RunMode::VERIFY){
                std::cerr << "hash-file does not exist\n";
                return -1;
            }
        }
    }

    int exitcode = 0;

    TreeHash::EventListener eventListener;
    eventListener.onFileProcessed = [loglevel, &exitcode, hashfileFromStdin](QString path, bool success) -> void{
        if(!success){
            if(loglevel >= 1){
                if(hashfileFromStdin)
                    std::cerr << QStringLiteral("file unsuccessful: %1\n").arg(path).toStdString();
                else
                    std::cout << QStringLiteral("file unsuccessful: %1\n").arg(path).toStdString();
            }

            if(exitcode < 1)
                exitcode = 1;
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
    eventListener.onError = [loglevel, &exitcode, hashfileFromStdin](QString msg, QString path) -> void{
        if(loglevel >= 1){
            if(hashfileFromStdin)
                std::cerr << QStringLiteral("ERROR: %1 @ %2\n").arg(msg, path).toStdString();
            else
                std::cout << QStringLiteral("ERROR: %1 @ %2\n").arg(msg, path).toStdString();
        }

        if(exitcode < 2)
            exitcode = 2;
    };

    try{
        TreeHash::LibTreeHash treeHash(eventListener);
        treeHash.setMode(mode);
        treeHash.setHashAlgorithm(hashAlg);
        treeHash.setRootDir(args.value("r"));
        treeHash.setFiles(listFiles(args));
        treeHash.setHmacKey(args.value("k"));

        if(hashfileFromStdin){
            auto in = std::make_unique<QFile>();
            auto out = std::make_unique<QFile>();

            if(!in->open(stdin, QFile::OpenModeFlag::ReadOnly, QFile::FileHandleFlag::DontCloseHandle)){
                std::cerr << QStringLiteral("unable to open stdin (%1)\n").arg(in->errorString()).toStdString();
                return -2;
            }
            if(!out->open(stdout, QFile::OpenModeFlag::WriteOnly, QFile::FileHandleFlag::DontCloseHandle)){
                std::cerr << QStringLiteral("unable to open stdout (%1)\n").arg(out->errorString()).toStdString();
                return -2;
            }

            treeHash.setHashesFile(std::move(in), std::move(out));
        }else{
            treeHash.setHashesFilePath(args.value("f"));
        }

        treeHash.run();

        return exitcode;
    }catch(std::exception& e){
        std::cerr << "LibTreeHash threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int execClean(QCommandLineParser& args){
    if(args.values("r").size() != 1){
        std::cerr << "root must me set exactly once\n";
        return -1;
    }
    if(args.values("f").size() != 1){
        std::cerr << "hashfile must me set exactly once\n";
        return -1;
    }

    QDir root(args.value("r"));
    if(!root.exists()){
        std::cerr << "root does not exist\n";
        return -1;
    }

    QFile hashfile(args.value("f"));
    if(!hashfile.exists()){
        std::cerr << "hash-file does not exist\n";
        return -1;
    }

    QStringList keep = listFiles(args);

    try{
        QString err;
        TreeHash::cleanHashFile(hashfile.fileName(), root.path(), keep, &err);

        if(!err.isNull()){
            std::cerr << "cleanHashFile returned with an error:\n" << err.toStdString() << '\n';
            return -2;
        }

        return 0;
    }catch(std::exception& e){
        std::cerr << "cleanHashFile threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int execRemoved(QCommandLineParser& args){
    if(args.values("r").size() != 1){
        std::cerr << "root must me set exactly once\n";
        return -1;
    }
    if(args.values("f").size() != 1){
        std::cerr << "hashfile must me set exactly once\n";
        return -1;
    }

    QDir root(args.value("r"));
    if(!root.exists()){
        std::cerr << "root does not exist\n";
        return -1;
    }

    QFile hashfile(args.value("f"));
    if(!hashfile.exists()){
        std::cerr << "hash-file does not exist\n";
        return -1;
    }

    QStringList existing = listFiles(args);

    try{
        QString err;
        QStringList missing = TreeHash::checkForRemovedFiles(hashfile.fileName(), root.path(), existing, &err);

        if(!err.isNull()){
            std::cerr << "cleanHashFile returned with an error:\n" << err.toStdString() << '\n';
            return -2;
        }

        // remove all excluded files from missing
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
        return 0;
    }catch(std::exception& e){
        std::cerr << "checkForRemovedFiles threw an exception:\n" << e.what() << '\n';
        return -2;
    }
}

int exec(QCommandLineParser& parser){
    // check for mandatory options
    if(!parser.isSet("r")){
        std::cerr << "option -r (root) is mandatory\n\n";
        parser.showHelp(-1);
        return -1;
    }
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
