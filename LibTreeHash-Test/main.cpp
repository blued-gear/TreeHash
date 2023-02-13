#include "tst_freshupdatetest.cpp"
#include "tst_verifytest.cpp"
#include "tst_partialupdatetest.cpp"

int main(int argc, char** argv){
    int status = 0;

    {
        FreshUpdateTest test;
        status |= QTest::qExec(&test, argc, argv);
    }

    {
        VerifyTest test;
        status |= QTest::qExec(&test, argc, argv);
    }

    {
        PartialUpdateTest test;
        status |= QTest::qExec(&test, argc, argv);
    }

    return status;
}
