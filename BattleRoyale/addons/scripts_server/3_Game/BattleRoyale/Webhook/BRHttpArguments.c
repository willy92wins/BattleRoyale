#ifdef SERVER
// Query argument of a webhook request: param1 is the name, param2 the value
typedef Param2<string, string> BRHttpArgument;

// Query arguments of a webhook request, in the order they are added
class BRHttpArguments: array<ref BRHttpArgument>
{
	// Appends the arguments to path as "?name=value&name=value", without encoding.
	// The webhook server expects this exact string, so the filtering below keeps the
	// historical behaviour: one forward pass that removes arguments with an empty name
	// or value; the entry that moves into a removed slot is not checked in that pass.
	string ToQuery(string path)
	{
		string query = path;

		int check_index = 0;
		while (check_index < Count())
		{
			BRHttpArgument checked_argument = Get(check_index);
			if (!checked_argument || checked_argument.param1 == string.Empty || checked_argument.param2 == string.Empty)
				RemoveOrdered(check_index);

			check_index++;
		}

		string separator = "?";
		int count = Count();
		for (int i = 0; i < count; i++)
		{
			BRHttpArgument argument = Get(i);
			// A null entry can only remain after the pass above; it adds nothing
			if (!argument)
				continue;

			query += separator + argument.param1 + "=" + argument.param2;
			separator = "&";
		}

		return query;
	}
}
#endif
