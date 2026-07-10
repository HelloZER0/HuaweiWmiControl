using Microsoft.Management.Infrastructure;
using HuaweiWmiControl.Services;
using HuaweiWmiControl.Wmi;

namespace HuaweiWmiControl.Tests;

/// <summary>
/// WmiConnectionManager 的测试桩——通过构造函数直接设置属性，无需真实 WMI 连接。
/// </summary>
public class StubWmiConnectionManager : WmiConnectionManager
{
    public StubWmiConnectionManager(bool available, CimSession? session, CimInstance? instance)
    {
        Available = available;
        _sessionField = session;
        _instanceField = instance;
    }

    public override bool Available { get; protected set; }
    public override string NotAvailableReason { get; protected set; } = "";
    public override CimSession Session => _sessionField!;
    public override CimInstance Instance => _instanceField!;
    public override IWmiProtocol Protocol => throw new InvalidOperationException("不应在测试中访问 Connection.Protocol");

    private readonly CimSession? _sessionField;
    private readonly CimInstance? _instanceField;
}

public class StubWmiProtocol : IWmiProtocol
{
    public string ProtocolName => "Test";
    public Func<ulong, byte[]> OnInvoke { get; set; } = _ => new byte[] { 0x00 };
    public CimMethodParametersCollection CreateInputParameters(ulong cmd) => new();
    public ulong EncodeCommand(ulong cmd, params byte[] args) => cmd;
    public byte[] Invoke(ulong cmd, CimSession session, CimInstance instance) => OnInvoke(cmd);
    public Task<byte[]> InvokeAsync(ulong cmd, CimSession session, CimInstance instance, CancellationToken ct = default)
        => Task.FromResult(OnInvoke(cmd));
}

/// <summary>
/// 测试辅助类——将 WmiServiceBase 的 protected 方法暴露为 public。
/// </summary>
public class TestWmiService : WmiServiceBase
{
    public TestWmiService(IWmiProtocol protocol, WmiConnectionManager connection)
        : base(protocol, connection) { }

    public new byte[] Call(ulong cmd) => base.Call(cmd);
    public new T? InvokeGet<T>(ulong cmd, Func<byte[], T> parser, params byte[] args) where T : struct
        => base.InvokeGet(cmd, parser, args);
    public new bool InvokeSet(ulong cmd, params byte[] args)
        => base.InvokeSet(cmd, args);
}

public class WmiServiceBaseTests
{
    private static StubWmiConnectionManager ConnectedStub() =>
        new(true, null!, null!);

    private static StubWmiConnectionManager DisconnectedStub() =>
        new(false, null, null);

    private static byte[] Success(params byte[] data) => new byte[] { 0x00 }.Concat(data).ToArray();
    private static byte[] Fail(byte status = 1) => new[] { status };

    [Fact]
    public void Call_OnFirstAttemptSuccess_ReturnsBytes()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Success(0x42) };
        var service = new TestWmiService(protocol, ConnectedStub());

        var result = service.Call(0x1234);

        Assert.Equal(new byte[] { 0x00, 0x42 }, result);
    }

    [Fact]
    public void Call_AfterTwoFailures_ThrowsWmiNotSupportedException()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Fail(0x01) };
        var service = new TestWmiService(protocol, ConnectedStub());

        var ex = Assert.Throws<WmiNotSupportedException>(() => service.Call(0x1234));
        Assert.Equal(0x1234ul, ex.Command);
    }

    [Fact]
    public void Call_WhenNotConnected_ThrowsInvalidOperationException()
    {
        var protocol = new StubWmiProtocol();
        var service = new TestWmiService(protocol, DisconnectedStub());

        Assert.Throws<InvalidOperationException>(() => service.Call(0x1234));
    }

    [Fact]
    public void InvokeGet_WhenProtocolSucceeds_ReturnsParsedValue()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Success(0x50) };
        var service = new TestWmiService(protocol, ConnectedStub());

        int? result = service.InvokeGet(0x1103, b => (int)b[1]);

        Assert.Equal(80, result);
    }

    [Fact]
    public void InvokeGet_WhenNotSupported_ReturnsNull()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Fail(0x01) };
        var service = new TestWmiService(protocol, ConnectedStub());

        int? result = service.InvokeGet(0x9999, b => (int)b[1]);

        Assert.Null(result);
        Assert.NotNull(service.LastError);
        Assert.IsType<WmiNotSupportedException>(service.LastError);
    }

    [Fact]
    public void InvokeGet_WhenSystemError_PropagatesException()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => throw new InvalidOperationException("连接已断开") };
        var service = new TestWmiService(protocol, ConnectedStub());

        Assert.Throws<InvalidOperationException>(() => service.InvokeGet(0x1103, b => (int)b[1]));
    }

    [Fact]
    public void InvokeSet_WhenProtocolSucceeds_ReturnsTrue()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Success() };
        var service = new TestWmiService(protocol, ConnectedStub());

        bool result = service.InvokeSet(0x1003, 0x32, 0x50);

        Assert.True(result);
    }

    [Fact]
    public void InvokeSet_WhenNotSupported_ReturnsFalse()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => Fail(0x01) };
        var service = new TestWmiService(protocol, ConnectedStub());

        bool result = service.InvokeSet(0x1003, 0x32, 0x50);

        Assert.False(result);
        Assert.IsType<WmiNotSupportedException>(service.LastError);
    }

    [Fact]
    public void InvokeSet_WhenSystemError_PropagatesException()
    {
        var protocol = new StubWmiProtocol { OnInvoke = _ => throw new InvalidOperationException("连接已断开") };
        var service = new TestWmiService(protocol, ConnectedStub());

        Assert.Throws<InvalidOperationException>(() => service.InvokeSet(0x1003, 0x32, 0x50));
    }
}
