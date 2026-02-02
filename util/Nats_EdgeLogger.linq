<Query Kind="Statements">
  <NuGetReference>NATS.Net</NuGetReference>
  <Namespace>NATS.Client.Core</Namespace>
  <Namespace>NATS.Net</Namespace>
  <Namespace>System.Threading.Tasks</Namespace>
</Query>

await using var nc = new NatsClient(new NatsOpts
	  {
		  Url = "nats://picollect.local:4222",
		  Name = "LINQPad",
		  AuthOpts = NatsAuthOpts.Default with
		  {
			  Username = Util.GetPassword("picollect.nats.username"),
			  Password = Util.GetPassword("picollect.nats.password")
		  }
	  });

Task subscription = Task.Run(async () =>
{
	await foreach (NatsMsg<string> msg in nc.SubscribeAsync<string>("aura2.logs.>", cancellationToken:ScriptCancelToken))
	{
		Console.WriteLine($"Received {msg.Subject}: {msg.Data}\n");
	}
});

// Give subscription time to start
await Task.Delay(Timeout.Infinite, ScriptCancelToken);

// Make sure subscription completes cleanly
await subscription;
