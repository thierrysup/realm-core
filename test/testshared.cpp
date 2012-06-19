#include <UnitTest++.h>
#include "tightdb.hpp"
#include "tightdb/group_shared.hpp"

// Does not work for windows yet
#ifndef _MSC_VER

using namespace tightdb;

TIGHTDB_TABLE_4(TestTableShared,
                first,  Int,
                second, Int,
                third,  Bool,
                fourth, String)

TEST(Shared_Initial)
{
    // Delete old files if there
    remove("test_shared.tdb");
    remove("test_shared.tdb.lock"); // also the info file

    {
        // Create a new shared db
        SharedGroup shared("test_shared.tdb");
        CHECK(shared.is_valid());

        // Verify that new group is empty
        {
            const Group& g1 = shared.begin_read();
            CHECK(g1.is_valid());
            CHECK(g1.is_empty());
            shared.end_read();
        }

#ifdef _DEBUG
        // Also do a basic ringbuffer test
        shared.test_ringbuf();
#endif
    }

    // Verify that lock file was deleted after use
    const int rc = access("test_shared.tdb.lock", F_OK);
    CHECK_EQUAL(-1, rc);
}

TEST(Shared1)
{
    // Delete old files if there
    remove("test_shared.tdb");
    remove("test_shared.tdb.lock"); // also the info file
    
    {
        // Create a new shared db
        SharedGroup shared("test_shared.tdb");
        CHECK(shared.is_valid());
        
        // Create first table in group
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(1, 2, false, "test");
            shared.commit();
        }
        
        // Open same db again
        SharedGroup shared2("test_shared.tdb");
        CHECK(shared2.is_valid());
        {
            const Group& g2 = shared2.begin_read();

            // Verify that last set of changes are commited
            TestTableShared::ConstRef t2 = g2.get_table<TestTableShared>("test");
            CHECK(t2->size() == 1);
            CHECK_EQUAL(1, t2[0].first);
            CHECK_EQUAL(2, t2[0].second);
            CHECK_EQUAL(false, t2[0].third);
            CHECK_EQUAL("test", (const char*)t2[0].fourth);
            // don't end_read yet

            // Do a new change while stil having current read transaction open
            {
                Group& g1 = shared.begin_write();
                TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
                t1->add(2, 3, true, "more test");
                shared.commit();
            }

            // Verify that that the read transaction does not see
            // the change yet (is isolated)
            CHECK(t2->size() == 1);
            CHECK_EQUAL(1, t2[0].first);
            CHECK_EQUAL(2, t2[0].second);
            CHECK_EQUAL(false, t2[0].third);
            CHECK_EQUAL("test", (const char*)t2[0].fourth);

            // Do one more new change while stil having current read transaction open
            // so we know that it does not overwrite data held by
            {
                Group& g1 = shared.begin_write();
                TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
                t1->add(0, 1, false, "even more test");
                shared.commit();
            }

            // Verify that that the read transaction does still not see
            // the change yet (is isolated)
            CHECK(t2->size() == 1);
            CHECK_EQUAL(1, t2[0].first);
            CHECK_EQUAL(2, t2[0].second);
            CHECK_EQUAL(false, t2[0].third);
            CHECK_EQUAL("test", (const char*)t2[0].fourth);

            // Close read transaction
            shared2.end_read();
        }

        // Start a new read transaction and verify that it can now see the changes
        {
            const Group& g3 = shared2.begin_read();
            TestTableShared::ConstRef t3 = g3.get_table<TestTableShared>("test");

            CHECK(t3->size() == 3);
            CHECK_EQUAL(1, t3[0].first);
            CHECK_EQUAL(2, t3[0].second);
            CHECK_EQUAL(false, t3[0].third);
            CHECK_EQUAL("test", (const char*)t3[0].fourth);
            CHECK_EQUAL(2, t3[1].first);
            CHECK_EQUAL(3, t3[1].second);
            CHECK_EQUAL(true, t3[1].third);
            CHECK_EQUAL("more test", (const char*)t3[1].fourth);
            CHECK_EQUAL(0, t3[2].first);
            CHECK_EQUAL(1, t3[2].second);
            CHECK_EQUAL(false, t3[2].third);
            CHECK_EQUAL("even more test", (const char*)t3[2].fourth);

            shared2.end_read();
        }
    }

    // Verify that lock file was deleted after use
    const int rc = access("test_shared.tdb.lock", F_OK);
    CHECK_EQUAL(-1, rc);
}

TEST(Shared_rollback)
{
    // Delete old files if there
    remove("test_shared.tdb");
    remove("test_shared.tdb.lock"); // also the info file

    {
        // Create a new shared db
        SharedGroup shared("test_shared.tdb");
        CHECK(shared.is_valid());

        // Create first table in group (but rollback)
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(1, 2, false, "test");
            shared.rollback();
        }

        // Verify that no changes were made
        {
            const Group& g1 = shared.begin_read();
            CHECK_EQUAL(false, g1.has_table("test"));
            shared.end_read();
        }

        // Really create first table in group
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(1, 2, false, "test");
            shared.commit();
        }

        // Verify that the changes were made
        {
            const Group& g1 = shared.begin_read();
            TestTableShared::ConstRef t = g1.get_table<TestTableShared>("test");
            CHECK(t->size() == 1);
            CHECK_EQUAL(1, t[0].first);
            CHECK_EQUAL(2, t[0].second);
            CHECK_EQUAL(false, t[0].third);
            CHECK_EQUAL("test", (const char*)t[0].fourth);
            shared.end_read();
        }

        // Greate more changes (but rollback)
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(0, 0, true, "more test");
            shared.rollback();
        }

        // Verify that no changes were made
        {
            const Group& g1 = shared.begin_read();
            TestTableShared::ConstRef t = g1.get_table<TestTableShared>("test");
            CHECK(t->size() == 1);
            CHECK_EQUAL(1, t[0].first);
            CHECK_EQUAL(2, t[0].second);
            CHECK_EQUAL(false, t[0].third);
            CHECK_EQUAL("test", (const char*)t[0].fourth);
            shared.end_read();
        }
    }

    // Verify that lock file was deleted after use
    const int rc = access("test_shared.tdb.lock", F_OK);
    CHECK_EQUAL(-1, rc);
}

TEST(Shared_Writes)
{
    // Delete old files if there
    remove("test_shared.tdb");
    remove("test_shared.tdb.lock"); // also the info file

    {
        // Create a new shared db
        SharedGroup shared("test_shared.tdb");
        CHECK(shared.is_valid());

        // Create first table in group
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(0, 2, false, "test");
            shared.commit();
        }

        // Do a lot of repeated write transactions
        for (size_t i = 0; i < 100; ++i) {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1[0].first += 1;
            shared.commit();
        }

        // Verify that the changes were made
        {
            const Group& g1 = shared.begin_read();
            TestTableShared::ConstRef t = g1.get_table<TestTableShared>("test");
            const int64_t v = t[0].first;
            CHECK_EQUAL(100, v);
            shared.end_read();
        }
    }

    // Verify that lock file was deleted after use
    const int rc = access("test_shared.tdb.lock", F_OK);
    CHECK_EQUAL(-1, rc);
}

namespace  {

void* IncrementEntry(void*);

void* IncrementEntry(void*)
{
    // Open shared db
    SharedGroup shared("test_shared.tdb");
    CHECK(shared.is_valid());

    for (size_t i = 0; i < 100; ++i) {
        Group& g1 = shared.begin_write();
        TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
        t1[0].first += 1;
        shared.commit();
    }
    return NULL;
}

} // namespace

TEST(Shared_WriterThreads)
{
    // Delete old files if there
    remove("test_shared.tdb");
    remove("test_shared.tdb.lock"); // also the info file

    {
        // Create a new shared db
        SharedGroup shared("test_shared.tdb");
        CHECK(shared.is_valid());

        // Create first table in group
        {
            Group& g1 = shared.begin_write();
            TestTableShared::Ref t1 = g1.get_table<TestTableShared>("test");
            t1->add(0, 2, false, "test");
            shared.commit();
        }

        const size_t thread_count = 10;
        pthread_t threads[thread_count];

        // Create all threads
        for (size_t i = 0; i < thread_count; ++i) {
            const int rc = pthread_create(&threads[i], NULL, IncrementEntry, NULL);
            CHECK_EQUAL(0, rc);
        }

        // Wait for all threads to complete
        for (size_t i = 0; i < thread_count; ++i) {
            const int rc = pthread_join(threads[i], NULL);
            CHECK_EQUAL(0, rc);
        }

        // Verify that the changes were made
        {
            const Group& g1 = shared.begin_read();
            TestTableShared::ConstRef t = g1.get_table<TestTableShared>("test");
            const int64_t v = t[0].first;
            const int64_t expected = 100 * thread_count;
            CHECK_EQUAL(expected, v);
            shared.end_read();
        }
    }

    // Verify that lock file was deleted after use
    const int rc = access("test_shared.tdb.lock", F_OK);
    CHECK_EQUAL(-1, rc);
}

#endif //_MSV_VER