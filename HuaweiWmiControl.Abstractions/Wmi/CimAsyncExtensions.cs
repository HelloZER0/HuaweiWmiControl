using System;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Management.Infrastructure.Generic;

namespace HuaweiWmiControl.Wmi
{
    /// <summary>
    /// 将 <see cref="CimAsyncResult{T}"/>（IObservable 模式）桥接为标准 <see cref="Task{T}"/>，
    /// 使 WMI 异步调用不再依赖 Task.Run 包装。
    /// </summary>
    internal static class CimAsyncExtensions
    {
        /// <summary>
        /// 将 CimAsyncResult 转换为可等待的 Task。
        /// 操作在 Subscribe 时启动，通过 IObserver 回调完成/错误。
        /// </summary>
        public static Task<T> AsTask<T>(this CimAsyncResult<T> source, CancellationToken ct = default)
        {
            if (source == null)
                throw new ArgumentNullException(nameof(source));

            var tcs = new TaskCompletionSource<T>(
                TaskCreationOptions.RunContinuationsAsynchronously);

            IDisposable? subscription = null;
            subscription = source.Subscribe(new CimAsyncObserver<T>(
                onNext: value =>
                {
                    tcs.TrySetResult(value);
                    subscription?.Dispose();
                },
                onError: error =>
                {
                    tcs.TrySetException(error);
                    subscription?.Dispose();
                },
                onCompleted: () =>
                {
                    if (!tcs.Task.IsCompleted)
                        tcs.TrySetException(new InvalidOperationException(
                            "CIM 异步操作异常完成：未收到结果也未收到错误。"));
                    subscription?.Dispose();
                }));

            // 注册取消：取消时中断订阅
            if (ct.CanBeCanceled)
            {
                ct.Register(() =>
                {
                    subscription?.Dispose();
                    tcs.TrySetCanceled(ct);
                });
            }

            return tcs.Task;
        }

        /// <summary>
        /// 简化的 IObserver{T} 实现，通过回调桥接 CimAsyncResult → Task。
        /// </summary>
        private sealed class CimAsyncObserver<T> : IObserver<T>
        {
            private readonly Action<T> _onNext;
            private readonly Action<Exception> _onError;
            private readonly Action _onCompleted;

            public CimAsyncObserver(Action<T> onNext, Action<Exception> onError, Action onCompleted)
            {
                _onNext = onNext ?? throw new ArgumentNullException(nameof(onNext));
                _onError = onError ?? throw new ArgumentNullException(nameof(onError));
                _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));
            }

            public void OnNext(T value) => _onNext(value);
            public void OnError(Exception error) => _onError(error);
            public void OnCompleted() => _onCompleted();
        }
    }
}
