using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace OperaTool
{
	class Util
	{
		static public UInt32 ArrayToUint32(Byte[] array)
		{
			// Reverse if we are on a little endian architecture.
			if (BitConverter.IsLittleEndian)
				Array.Reverse(array);

			return BitConverter.ToUInt32(array, 0);
		}

		static public Int32 ArrayToInt32(Byte[] array)
		{
			// Reverse if we are on a little endian architecture.
			if (BitConverter.IsLittleEndian)
				Array.Reverse(array);

			return BitConverter.ToInt32(array, 0);
		}

		static public Byte[] UInt32ToArray(UInt32 number)
		{
			Byte[] array = BitConverter.GetBytes(number);

			// Reverse if we are on a little endian architecture.
			if (BitConverter.IsLittleEndian)
				Array.Reverse(array);

			return array;
		}

		static public Byte[] Int32ToArray(Int32 number)
		{
			Byte[] array = BitConverter.GetBytes(number);

			// Reverse if we are on a little endian architecture.
			if (BitConverter.IsLittleEndian)
				Array.Reverse(array);

			return array;
		}
	}
}
