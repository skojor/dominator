#include "datastream.h"

#include <QtTest>

using namespace DataStream;

class DataStreamTest : public QObject
{
    Q_OBJECT

private slots:
    void refillsAndInterpolatesAcrossNorth()
    {
        Simulator backend(2, 0.02);
        Session session(backend, limits);
        QVERIFY(session.start({{1, 359, 10}, {2, 361, 12}, {3, 363, 12}}, 0));
        for (int i = 1; i <= 150; ++i)
            session.tick(i / 100.0);
        const auto point = backend.pointing();
        QVERIFY(point);
        QCOMPARE(point->azimuth, 360.0);
        QCOMPARE(point->elevation, 11.0);
        for (int i = 151; i <= 320; ++i)
            session.tick(i / 100.0);
        QVERIFY(session.state() == Session::State::Completed);
    }

    void confirmsAbort()
    {
        Simulator backend(3);
        Session session(backend, limits);
        QVERIFY(session.start({{1, 10, 10}, {2, 10, 11}, {3, 8, 11}}, 0));
        session.tick(0.1);
        session.abort();
        session.tick(0.2);
        session.tick(0.3);
        QVERIFY(session.state() == Session::State::Aborted);
        backend.advance(3);
        QVERIFY(!backend.pointing());
    }

    void faultsWithoutRestart()
    {
        for (int scenario = 0; scenario < 6; ++scenario) {
            Simulator backend(2);
            Session session(backend, limits);
            QVERIFY(session.start({{1, 10, 10}, {2, 12, 10}}, 0));
            backend.dropNextReply = scenario == 0;
            backend.rejectNext = scenario == 1;
            backend.corruptNextReply = scenario == 2;
            session.tick(0.1);
            backend.online = scenario != 3;
            if (scenario == 4)
                session.tick(-1);
            const double start = scenario == 5 ? 2.0 : 0.2;
            for (int i = 0; i < 10; ++i)
                session.tick(start + i * 0.25);
            QVERIFY(session.state() == Session::State::Fault);
            backend.online = true;
            QVERIFY(!session.start({{10, 10, 10}}, 4));
        }
    }

    void rejectsUnwrappedDiscontinuity()
    {
        Simulator backend(2);
        Session session(backend, limits);
        QVERIFY(!session.start({{1, 359, 10}, {2, 1, 10}}, 0));
        QVERIFY(session.state() == Session::State::Fault);
    }

private:
    const Limits limits{0, 400, 0, 90, 5, 5, 1.8};
};

QTEST_GUILESS_MAIN(DataStreamTest)
#include "datastream_test.moc"
