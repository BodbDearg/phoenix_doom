using System;
using System.Collections.Generic;
using System.IO;
using System.Linq; // Evil!
using System.Text;

/*
 * Based on info found at:
 * http://hackipedia.org/Platform/3D0/html,%203DO%20SDK%20Documentation/Type%20A/ppgfldr/smmfldr/cdmfldr/00CDMTOC.html
 *
 */

// Special files:
//	Disc label
//	Catapult

// file/folder name max size is 32.

// Things You Should Do:
//	Issue reads in fairly big chunks (32K or larger) wherever possible.
//	When possible, use read-ahead buffers before branches to reduce branching seek delays.
//	Provide adequate buffering to survive a one-revolution delay (up to 150 milliseconds).
//	If files are used together (e.g., in one level of a game), put them in the same directory.
//	If files are accessed in a strictly sequential manner, name them in ascending order.
//	Use data-streaming techniques to combine multiple types of data into a single file which can be read sequentially. This eliminates seeks between files.

// Layout Optimizer:
//	Attempts to cluster files together on the disc in the same order that they're used during program testing.
//	Places one avatar of each file into cluster where it's used most frequently.
//	Attempts to place additional avatars of commonly-used files into other clusters which use the file.
//	Organizes clusters with most seeks nearest center to reduce latency.
//	Attempts to place directory avatars near files.
//	Is very effective at reducing the number of seeks and shortening those seeks which do occur.
//	Does not guarantee "zero seek" performance.
//	Greatly improves the titles initial load time by interleaving the files needed for booting (Catapult).

namespace OperaTool
{
  #region enums

  public enum EntryFlags
  {
    None = 0,
    File = 1 << 0,
    SpecialFile = 1 << 1,
    Directory = 1 << 2,
    LastEntryInBlock = 1 << 3,
    LastEntryInDir = 1 << 4
  }

  public enum BlockType
  {
    None = 0,
    Root = 1 << 0,
    Header = 1 << 1,
    Data = 1 << 2,
  }

  #endregion enums

  #region structs

  public struct VolumeHeader
  {
    public Byte recordType; // Always 1.
    public Byte[] syncBytes; // Always 0x5A.
    public Byte structureVersion; // Always 1.
    public Byte flags;
    public Byte[] comment;
    public Byte[] label;
    public UInt32 identifier;
    public UInt32 blockSize; // Always 2048 byte sectors.
    public UInt32 blockCount; // Total number of blocks on the disk.
  }

  public struct RootHeader
  {
    public UInt32 id;
    public UInt32 blockCount;
    public UInt32 blockSize; // Always 2048.
    public UInt32 avatarsCount; // Avatars count (total amount = avatarsCount + 1, since the original isn't counted as an avatar).
    public UInt32[] avatars; // Locations of the header, in blocks, counted from the beginning of the disk.
  }

  public struct DirectoryHeader
  {
    public UInt32 nextBlock; // Next block in this directory (always 0xffffffff for the last block).
    public UInt32 prevBlock; // Previous block in this directory (always 0xffffffff for the first block).
    public UInt32 flags;
    public UInt32 firstFreeByte; // Offset from the beginning of the block to the first unused byte in the block.
    public UInt32 firstEntryOffset; // Offset from the beginning of the block to the first directory entry in this block (always 0x14).
  }

  public struct EntryHeader
  {
    public UInt32 flags; // Related to EntryFlags.
    public UInt32 id;
    public Byte[] type; // Seems to be the last 4 letters of the extension, right padded with spaces if less.
    public UInt32 blockSize; // Always 2048.
    public UInt32 byteCount; // Length of the entry in bytes.
    public UInt32 blockCount; // Length of the entry in blocks.
    public UInt32 burst; // Always 1.
    public UInt32 gap; // Always 0.
    public Byte[] filename; // Padded with '\0'.
    public UInt32 avatarsCount; // Avatars count (total amount = avatarsCount + 1, since the original isn't counted as an Avatar).
    public UInt32[] avatars; // Locations in blocks, counted from the beginning of the disk.
  }

  public struct BlockInfo
  {
    public BlockType type;
    public DirectoryHeader directoryHeader;
    public List<EntryHeader> entryHeaders;
    public Byte[] data;
  }

  #endregion structs

  #region classes

  public class Opera
  {
    #region private variables

    private const int m_maxEntryHeadersPerBlock = 28; // Failsafe: Always considers entries as having a full avatar array.
    private const int m_discLabelByteCount = 132;
    private const int m_blockSize = 2048;

    private const int m_directoryAvatarsCount = 3; // May be safely reduced to 2.
    private const int m_rootAvatarsCount = 7; // May be safely reduced to 2.
    private const int m_labelAvatarsCount = 2; // Must not be changed.

    private const int m_syncSize = 5;
    private const int m_volumeCommentSize = 32;
    private const int m_volumeLabelSize = 32;
    private const int m_entryTypeSize = 4;
    private const int m_filenameSize = 32;
    private const String m_separator = "\\";
    private readonly Byte[] m_duckToken = Encoding.ASCII.GetBytes("iamaduck");

    private BinaryReader m_br = null;
    private BinaryWriter m_bw = null;

    private VolumeHeader m_volumeHeader = new VolumeHeader();
    private RootHeader m_rootHeader = new RootHeader();
    private List<DirectoryHeader> m_directoryHeaderList = new List<DirectoryHeader>();
    private List<EntryHeader> m_entryHeaderList = new List<EntryHeader>();

    private Dictionary<UInt32, BlockInfo> m_blocks = new Dictionary<UInt32, BlockInfo>();

    private Dictionary<UInt32, BlockInfo> m_headerBlocks = new Dictionary<UInt32, BlockInfo>();
    private Dictionary<UInt32, BlockInfo> m_dataBlocks = new Dictionary<UInt32, BlockInfo>();
    private Dictionary<UInt32, UInt32> m_blockById = new Dictionary<UInt32, UInt32>();

    private UInt32 m_currentFSIndex = 0;

    #endregion private variables

    #region public methods

    public void DecompileFS(String filename, String outputFolder)
    {
      try
      {
        // Open file with BinaryReader.
        m_br = new BinaryReader(File.Open(filename, FileMode.Open));

        // Read volume header record.
        ReadVolumeHeader();

        //Console.Write(Environment.NewLine);
        //Console.WriteLine("Root directory:");

        // Root directory.
        ReadRootDirectory();

        // Read directory headers and entries.
        ReadDirectory(m_rootHeader.avatars[0], 0, outputFolder);

        Console.WriteLine("Done.");
      }
      catch (Exception ex)
      {
        Console.Error.WriteLine(ex);
      }
    }

    public void CompileFS(String path, String filename)
    {
      try
      {
        ConvertFilesToBlocks(path);

        OrganizeBlocks();

        SaveBlocks(filename);

        Console.WriteLine("Done.");
      }
      catch (Exception ex)
      {
        Console.Error.WriteLine(ex);
      }
    }

    public void CompileFSFromMemory(String filename)
    {
      try
      {
        FileStream fs = new FileStream(filename, FileMode.Create);
        m_bw = new BinaryWriter(fs);

        //-----------------------------
        // The first block is special.

        // Volume header.
        m_bw.Write(m_volumeHeader.recordType);
        m_bw.Write(m_volumeHeader.syncBytes);
        m_bw.Write(m_volumeHeader.structureVersion);
        m_bw.Write(m_volumeHeader.flags);
        m_bw.Write(m_volumeHeader.comment);
        m_bw.Write(m_volumeHeader.label);
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.identifier));
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.blockSize));
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.blockCount));

        // Root header.
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.id));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.blockCount));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.blockSize));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.avatarsCount));
        for (int i = 0; i < m_rootHeader.avatarsCount + 1; i++)
          m_bw.Write(Util.UInt32ToArray(m_rootHeader.avatars[i]));

        // Null ended.
        m_bw.Write(0);

        // Duck this block.
        WriteDuck();

        //-----------------------------
        // Write standard blocks.

        for (UInt32 i = 1; i < m_volumeHeader.blockCount;)
        {
          if (m_blocks.ContainsKey(i) == true)
          {
            BlockInfo block = m_blocks[i];
            if (block.type == BlockType.Data)
            {
              //Console.WriteLine("Block " + i + ": Data");

              // Write file data.
              m_bw.Write(block.data);

              // Duck final block.
              WriteDuck();

              UInt32 blockCount = (UInt32)((block.data.Length + m_volumeHeader.blockSize - 1) / m_volumeHeader.blockSize);

              i += blockCount;

              //Console.WriteLine("size: " + blockCount);
            }
            else if (block.type == BlockType.Header)
            {
              //Console.WriteLine("Block " + i + ": Header");

              // Directory header.
              m_bw.Write(Util.UInt32ToArray(block.directoryHeader.nextBlock));
              m_bw.Write(Util.UInt32ToArray(block.directoryHeader.prevBlock));
              m_bw.Write(Util.UInt32ToArray(block.directoryHeader.flags));
              m_bw.Write(Util.UInt32ToArray(block.directoryHeader.firstFreeByte));
              m_bw.Write(Util.UInt32ToArray(block.directoryHeader.firstEntryOffset));

              // Entry headers.
              for (int j = 0; j < block.entryHeaders.Count; j++)
              {
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].flags));
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].id));

                for (int k = 0; k < block.entryHeaders[j].type.Length; k++)
                  m_bw.Write(block.entryHeaders[j].type[k]);

                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].blockSize));
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].byteCount));
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].blockCount));
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].burst));
                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].gap));

                for (int k = 0; k < block.entryHeaders[j].filename.Length; k++)
                  m_bw.Write(block.entryHeaders[j].filename[k]);

                m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].avatarsCount));

                for (int k = 0; k < block.entryHeaders[j].avatars.Length; k++)
                  m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].avatars[k]));
              }

              // Duck this block.
              WriteDuck();

              i++;
            }
            else
            {
              // FAIL block?
              Console.WriteLine("Block " + i + ": FAIL");
              WriteDuck();
              i++;
            }
          }
          else
          {
            // Empty block.
            Console.WriteLine("Block " + i + ": Empty");
            WriteDuck();
            i++;
          }
        }

        //----------
        m_bw.Close();
        fs.Close();
      }
      catch (Exception ex)
      {
        Console.Error.WriteLine(ex);
      }
    }

    #endregion public methods

    #region private methods

    private void WriteDuck()
    {
      long position = m_bw.BaseStream.Position;
      long finalPosition = ((position / m_volumeHeader.blockSize) + 1) * m_volumeHeader.blockSize;
      while (position++ < finalPosition)
        m_bw.Write(m_duckToken[(position - 1) % m_duckToken.Length]);
    }

    private UInt32 GetNewFSIndex()
    {
      return m_currentFSIndex++;
    }

    private void WriteDiscLabelFile()
    {
      // TODO...
    }

    private void WriteCatapultFile()
    {
      // TODO...
    }

    private void CreateNewHeaderBlock(UInt32 blockNumber, UInt32 nextBlock, UInt32 prevBlock)
    {
      BlockInfo newBlock = new BlockInfo();
      newBlock.type = BlockType.Header;
      newBlock.directoryHeader = new DirectoryHeader();
      newBlock.directoryHeader.nextBlock = nextBlock;
      newBlock.directoryHeader.prevBlock = prevBlock;
      newBlock.directoryHeader.flags = 0;
      newBlock.directoryHeader.firstFreeByte = 2044; // TODO: Is this really necessary?
      newBlock.directoryHeader.firstEntryOffset = 0x14;

      m_headerBlocks[blockNumber] = newBlock;
    }

    private void AddEntryToHeaderBlock(EntryHeader eh, UInt32 blockNumber)
    {
      BlockInfo block = m_headerBlocks[blockNumber];

      if (block.entryHeaders == null)
        block.entryHeaders = new List<EntryHeader>();

      block.entryHeaders.Add(eh);
      m_headerBlocks[blockNumber] = block;
    }

    private Byte[] GetFilenameAsArray(String filename)
    {
      Byte[] array = new Byte[m_filenameSize];

      if (filename.Length <= m_filenameSize)
      {
        // Initialize array with padding.
        for (int i = 0; i < array.Length; i++)
          array[i] = (Byte)'\0';

        // Set filename into array.
        char[] filenameArr = filename.ToCharArray();
        for (int i = 0; i < filenameArr.Length; i++)
          array[i] = (Byte)filenameArr[i];
      }
      else
      {
        Console.WriteLine("Error: Entry filename too long.");
      }

      return array;
    }

    private Byte[] GetEntryTypeAsArray(String type)
    {
      Byte[] array = new Byte[m_entryTypeSize];

      // Initialize array with padding.
      for (int i = 0; i < array.Length; i++)
        array[i] = (Byte)' ';

      // Set type into array.
      char[] filenameArr = type.ToCharArray();
      int size = filenameArr.Length > m_entryTypeSize ? m_entryTypeSize : filenameArr.Length;
      for (int i = 0; i < size; i++)
        array[i] = (Byte)filenameArr[i];

      return array;
    }

    // -first free byte
    // -root header
    // -write to file
    // -profit

    private void SaveBlocks(String filename)
    {
      try
      {
        FileStream fs = new FileStream(filename, FileMode.Create);
        m_bw = new BinaryWriter(fs);

        //-----------------------------
        // The first block is special.

        m_volumeHeader.recordType = 1;
        m_volumeHeader.syncBytes = new Byte[] { 0x5A, 0x5A, 0x5A, 0x5A, 0x5A };
        m_volumeHeader.structureVersion = 1;
        m_volumeHeader.flags = 0;
        m_volumeHeader.comment = new Byte[m_volumeCommentSize];
        m_volumeHeader.label = new Byte[m_volumeLabelSize];
        m_volumeHeader.label[0] = (Byte)'C';
        m_volumeHeader.label[1] = (Byte)'D';
        m_volumeHeader.label[2] = (Byte)'-';
        m_volumeHeader.label[3] = (Byte)'R';
        m_volumeHeader.label[4] = (Byte)'O';
        m_volumeHeader.label[5] = (Byte)'M';
        m_volumeHeader.identifier = 1366613; // TODO?
        m_volumeHeader.blockSize = m_blockSize;
        m_volumeHeader.blockCount = 1 + (UInt32)m_headerBlocks.Keys.Last() + (UInt32)m_dataBlocks.Keys.Last();

        m_rootHeader.id = 666; // TODO?
        m_rootHeader.blockCount = 1; // TODO!!!
        m_rootHeader.blockSize = m_blockSize;
        m_rootHeader.avatarsCount = 7; // TODO?
        m_rootHeader.avatars = new UInt32[] { 1, 1, 1, 1, 1, 1, 1, 1 }; // TODO?

        // Volume header.
        m_bw.Write(m_volumeHeader.recordType);
        m_bw.Write(m_volumeHeader.syncBytes);
        m_bw.Write(m_volumeHeader.structureVersion);
        m_bw.Write(m_volumeHeader.flags);
        m_bw.Write(m_volumeHeader.comment);
        m_bw.Write(m_volumeHeader.label);
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.identifier));
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.blockSize));
        m_bw.Write(Util.UInt32ToArray(m_volumeHeader.blockCount));

        // Root header.
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.id));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.blockCount));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.blockSize));
        m_bw.Write(Util.UInt32ToArray(m_rootHeader.avatarsCount));
        for (int i = 0; i < m_rootHeader.avatarsCount + 1; i++)
          m_bw.Write(Util.UInt32ToArray(m_rootHeader.avatars[i]));

        // Null ended.
        m_bw.Write(0);

        // Duck this block.
        WriteDuck();

        //Console.WriteLine("Blocks: " + m_volumeHeader.blockCount);
        //Console.WriteLine("Block " + 0 + ": Volume and Root Header");

        //-----------------------------
        // Write standard blocks.

        foreach (KeyValuePair<uint, BlockInfo> headerBlock in m_headerBlocks)
        {
          BlockInfo block = headerBlock.Value;

          // Directory header.
          m_bw.Write(Util.UInt32ToArray(block.directoryHeader.nextBlock));
          m_bw.Write(Util.UInt32ToArray(block.directoryHeader.prevBlock));
          m_bw.Write(Util.UInt32ToArray(block.directoryHeader.flags));
          m_bw.Write(Util.UInt32ToArray(block.directoryHeader.firstFreeByte));
          m_bw.Write(Util.UInt32ToArray(block.directoryHeader.firstEntryOffset));

          // Entry headers.
          for (int j = 0; j < block.entryHeaders.Count; j++)
          {
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].flags));
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].id));

            m_bw.Write(block.entryHeaders[j].type);

            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].blockSize));
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].byteCount));
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].blockCount));
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].burst));
            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].gap));

            for (int k = 0; k < block.entryHeaders[j].filename.Length; k++)
              m_bw.Write(block.entryHeaders[j].filename[k]);

            //for (int k = 0; k < block.entryHeaders[j].filename.Length; k++)
            //  Console.Write((char)block.entryHeaders[j].filename[k]);
            //Console.WriteLine("");

            m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].avatarsCount));

            for (int k = 0; k < block.entryHeaders[j].avatars.Length; k++)
              m_bw.Write(Util.UInt32ToArray(block.entryHeaders[j].avatars[k]));
          }

          // Duck this block.
          WriteDuck();
        }

        foreach (KeyValuePair<uint, BlockInfo> dataBlock in m_dataBlocks)
        {
          BlockInfo block = dataBlock.Value;

          // Write file data.
          m_bw.Write(block.data);

          // Duck final block.
          WriteDuck();
        }

        //----------
        m_bw.Close();
        fs.Close();
      }
      catch (Exception ex)
      {
        Console.Error.WriteLine(ex);
      }
    }

    private void OrganizeBlocks()
    {
      UInt32 indexFile = 0;
      UInt32[] dataBlocks = m_dataBlocks.Keys.ToArray();
      UInt32 dataBlockOffset = (UInt32)m_headerBlocks.Count + 1;

      List<UInt32> keys = new List<UInt32>(m_headerBlocks.Keys);
      foreach (UInt32 i in keys)
      {
        BlockInfo block = m_headerBlocks[i];
        if (block.entryHeaders != null)
        {
          for (int j = 0; j < block.entryHeaders.Count; j++)
          {
            EntryHeader entry = block.entryHeaders[j];

            // Don't do anything with directories, Disc label file or catapult file.
            if (Encoding.ASCII.GetString(entry.type).Equals("*dir") == false &&
                Encoding.ASCII.GetString(entry.type).Equals("*lbl") == false &&
                Encoding.ASCII.GetString(entry.type).Equals("*zap") == false)
            {
              entry.avatars = new UInt32[1];
              entry.avatars[0] = dataBlockOffset + dataBlocks[indexFile];

              block.entryHeaders[j] = entry;

              if (Encoding.ASCII.GetString(entry.filename).Contains("rom_tags") == true)
                Console.WriteLine("rom_tags position: " + entry.avatars[0]);

              indexFile++;
            }
          }
        }
        m_headerBlocks[i] = block;

        // TODO: Set directoryHeader.firstFreeByte HERE.
      }
    }

    private void ConvertFilesToBlocks(String path)
    {
      m_currentFSIndex = 413666134; // Starting index.

      UInt32 currentHBlock = 1; // Header block.
      UInt32 currentDBlock = 0; // Data block.
      bool isFirstBlock = true;

      //-------------------------------------------------
      // Special file: Disc label.

      CreateNewHeaderBlock(currentHBlock, 0xffffffff, 0xffffffff);

      EntryHeader ehLabel = new EntryHeader();

      ehLabel.flags = 0x06; // Special file.
      ehLabel.id = GetNewFSIndex();
      ehLabel.type = GetEntryTypeAsArray("*lbl");
      ehLabel.blockSize = m_blockSize;
      ehLabel.byteCount = m_discLabelByteCount;
      ehLabel.blockCount = 1;
      ehLabel.burst = 1;
      ehLabel.gap = 0;
      ehLabel.filename = GetFilenameAsArray("Disc label");
      ehLabel.avatarsCount = 0;
      ehLabel.avatars = new UInt32[] { 0 };

      // Add to current block.
      AddEntryToHeaderBlock(ehLabel, currentHBlock);

      //-------------------------------------------------
      // Headers.
      int dirCountTotal = 0;
      int fileCountTotal = 0;

      UInt32 avatarPosition = 2;

      Queue<String> queue = new Queue<String>();
      queue.Enqueue(path);
      while (queue.Count > 0)
      {
        path = queue.Dequeue();

        try
        {
          int totalEntriesCount = Directory.GetDirectories(path).Length + Directory.GetFiles(path).Length;
          UInt32 blockOffset = 0;
          int indexEntry = 1;
          int totalIndexEntry = 1;

          // Create new header block (only after block, because of Disc label).
          if (isFirstBlock == false)
          {
            currentHBlock++;
            CreateNewHeaderBlock(currentHBlock, 0xffffffff, 0xffffffff);
          }
          else
          {
            isFirstBlock = false;
          }

          UInt32 blockCountAvatarOffset = (UInt32)((totalEntriesCount + m_maxEntryHeadersPerBlock - 1) / m_maxEntryHeadersPerBlock);

          String lastIndexDir = String.Empty;

          // Directories.
          foreach (String subDir in Directory.GetDirectories(path))
          {
            queue.Enqueue(subDir);
            //Console.WriteLine(subDir);

            dirCountTotal++;

            int currentEntriesCount = Directory.GetDirectories(subDir).Length + Directory.GetFiles(subDir).Length;

            // Create entry.
            EntryHeader eh = new EntryHeader();

            eh.flags = 0x07;
            eh.id = GetNewFSIndex();
            eh.type = GetEntryTypeAsArray("*dir");
            eh.blockSize = m_blockSize;
            eh.blockCount = (UInt32)((currentEntriesCount + m_maxEntryHeadersPerBlock - 1) / m_maxEntryHeadersPerBlock);
            eh.byteCount = (UInt32)(eh.blockCount * m_blockSize);
            eh.burst = 1;
            eh.gap = 0;
            eh.filename = GetFilenameAsArray(Path.GetFileName(subDir));
            eh.avatarsCount = 0; // Remember: must add actual avatar array later.
            //eh.avatars = // Remember: must add actual avatar array later.
            eh.avatars = new UInt32[1];

            //----------------------------
            // Calculate avatar position.

            eh.avatars[0] = avatarPosition;

            avatarPosition += eh.blockCount;

            if (eh.blockCount * m_maxEntryHeadersPerBlock == currentEntriesCount)
              avatarPosition++;

            //----------------------------

            // Check if this is the last dir entry of the dir.
            if (totalIndexEntry == totalEntriesCount)
            {
              eh.flags |= 0x80000000;
              eh.flags |= 0x40000000;

              //avatarPosition += eh.blockCount;
            }
            // Check if this is the last dir entry in the block.
            else if (indexEntry + 1 == m_maxEntryHeadersPerBlock)
            {
              eh.flags |= 0x40000000;
            }

            if (indexEntry == m_maxEntryHeadersPerBlock)
            {
              indexEntry = 1;
              blockOffset++;

              // Set the info about the new block in the older one.
              BlockInfo oldBlock = m_headerBlocks[currentHBlock];
              oldBlock.directoryHeader.nextBlock = blockOffset;
              m_headerBlocks[currentHBlock] = oldBlock;

              // Create a new block and add it.
              currentHBlock++;
              CreateNewHeaderBlock(currentHBlock, 0xffffffff, blockOffset - 1);
            }

            // Add to current block.
            AddEntryToHeaderBlock(eh, currentHBlock);

            totalIndexEntry++;
            indexEntry++;
          }

          // Files.
          foreach (String file in Directory.GetFiles(path))
          {
            fileCountTotal++;

            //Console.WriteLine(file);

            // Create data block.
            BlockInfo dataBlock = new BlockInfo();
            dataBlock.data = File.ReadAllBytes(file);
            m_dataBlocks[currentDBlock] = dataBlock;
            currentDBlock += (UInt32)(dataBlock.data.Length / m_blockSize) + 1;

            // Create entry.
            EntryHeader eh = new EntryHeader();

            eh.flags = 0x02;
            eh.id = GetNewFSIndex();
            eh.type = GetEntryTypeAsArray(Path.GetExtension(file));
            eh.blockSize = m_blockSize;
            eh.byteCount = (UInt32)dataBlock.data.Length;
            eh.blockCount = (UInt32)((dataBlock.data.Length + m_blockSize - 1) / m_blockSize);
            eh.burst = 1;
            eh.gap = 0;
            eh.filename = GetFilenameAsArray(Path.GetFileName(file));
            eh.avatarsCount = 0; // Remember: must add actual avatar array later.
                                 //eh.avatars = // Remember: must add actual avatar array later.
                                 // TODO...

            // Check if this is the last dir entry of the dir.
            if (totalIndexEntry == totalEntriesCount)
            {
              eh.flags |= 0x80000000;
              eh.flags |= 0x40000000;
            }
            // Check if this is the last dir entry in the block.
            else if (indexEntry + 1 == m_maxEntryHeadersPerBlock)
            {
              eh.flags |= 0x40000000;
            }

            if (indexEntry == m_maxEntryHeadersPerBlock)
            {
              indexEntry = 1;
              blockOffset++;

              //avatarPosition++;

              // Set the info about the new block in the older one.
              BlockInfo oldBlock = m_headerBlocks[currentHBlock];
              oldBlock.directoryHeader.nextBlock = blockOffset;
              m_headerBlocks[currentHBlock] = oldBlock;

              // Create a new block and add it.
              currentHBlock++;
              CreateNewHeaderBlock(currentHBlock, 0xffffffff, blockOffset - 1);
            }

            // Add to current block.
            AddEntryToHeaderBlock(eh, currentHBlock);

            totalIndexEntry++;
            indexEntry++;
          }
        }
        catch (Exception ex)
        {
          Console.Error.WriteLine(ex);
        }
      }

      Console.WriteLine("dirCountTotal: " + dirCountTotal);
      Console.WriteLine("fileCountTotal: " + fileCountTotal);
    }

    private void ReadVolumeHeader()
    {
      // Record type.
      m_volumeHeader.recordType = m_br.ReadByte();

      // Synchronization bytes.
      m_volumeHeader.syncBytes = m_br.ReadBytes(m_syncSize);

      // Record version.
      m_volumeHeader.structureVersion = m_br.ReadByte();

      // Volume flags.
      m_volumeHeader.flags = m_br.ReadByte();

      // Volume comment.
      m_volumeHeader.comment = m_br.ReadBytes(m_volumeCommentSize);

      // Volume label.
      m_volumeHeader.label = m_br.ReadBytes(m_volumeLabelSize);

      // Volume identifier.
      m_volumeHeader.identifier = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Block size.
      m_volumeHeader.blockSize = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Block count.
      m_volumeHeader.blockCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
    }

    private void ReadRootDirectory()
    {
      Console.WriteLine("Root:");

      // Directory identifier.
      m_rootHeader.id = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
      Console.WriteLine("Directory identifier: " + m_rootHeader.id);

      // Block count.
      m_rootHeader.blockCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
      Console.WriteLine("Block count: " + m_rootHeader.blockCount);

      // Block size.
      m_rootHeader.blockSize = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Avatars count (total amount = avatarsCount + 1, since the original isn't counted as an Avatar).
      m_rootHeader.avatarsCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // locations of the header, in blocks, counted from the beginning of the disk.
      m_rootHeader.avatars = new UInt32[m_rootHeader.avatarsCount + 1];

      for (int i = 0; i < m_rootHeader.avatarsCount + 1; i++)
      {
        m_rootHeader.avatars[i] = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
      }
    }

    private void ReadDirectory(UInt32 initialBlock, UInt32 offsetBlocks, String currentPath)
    {
      DirectoryHeader dh = new DirectoryHeader();

      UInt32 currentBlock = initialBlock + offsetBlocks;

      m_br.BaseStream.Seek(currentBlock * m_rootHeader.blockSize, SeekOrigin.Begin);

      // Directory header.

      // Next block in this directory (always 0xffffffff for the last block).
      dh.nextBlock = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Previous block in this directory (always 0xffffffff for the first block).
      dh.prevBlock = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Flags.
      dh.flags = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Offset from the beginning of the block to the first unused byte in the block.
      dh.firstFreeByte = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Offset from the beginning of the block to the first directory entry in this block (always (?) 0x14).
      dh.firstEntryOffset = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

      // Add directory header to the list.
      m_directoryHeaderList.Add(dh);

      //--
      if (m_blocks.ContainsKey(currentBlock) == false)
        m_blocks[currentBlock] = new BlockInfo();

      BlockInfo block = m_blocks[currentBlock];
      block.type = BlockType.Header;
      block.directoryHeader = dh;

      m_blocks[currentBlock] = block;
      //--

      // Read directory entries.
      ReadEntry(currentBlock, dh.firstEntryOffset, currentPath);

      // Go to next block.
      if (dh.nextBlock != 0xffffffff)
        ReadDirectory(initialBlock, dh.nextBlock, currentPath);
    }

    private void ReadEntry(UInt32 currentBlock, UInt32 offset, String currentPath)
    {
      m_br.BaseStream.Seek((currentBlock * m_rootHeader.blockSize) + offset, SeekOrigin.Begin);

      // Check creation of directory.
      if (Directory.Exists(currentPath) == false)
        Directory.CreateDirectory(currentPath);

      int entrycount = 0;

      bool lastEntry = false;
      while (lastEntry == false)
      {
        // Entry.
        //Console.Write(Environment.NewLine);
        //Console.WriteLine("Entry: ");

        EntryHeader eh = new EntryHeader();

        // Flags.
        eh.flags = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Convert flags.
        EntryFlags entryFlags = EntryFlags.None;

        if ((eh.flags & 0x80000000) == 0x80000000)
          entryFlags |= EntryFlags.LastEntryInDir;
        if ((eh.flags & 0x40000000) == 0x40000000)
          entryFlags |= EntryFlags.LastEntryInBlock;

        if ((eh.flags & 0x07) == 0x07) entryFlags |= EntryFlags.Directory;
        else if ((eh.flags & 0x06) == 0x06) entryFlags |= EntryFlags.SpecialFile;
        else if ((eh.flags & 0x02) == 0x02) entryFlags |= EntryFlags.File;

        if (entryFlags.HasFlag(EntryFlags.LastEntryInDir) == true)
          lastEntry = true;

        // Identifier.
        eh.id = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Type.
        eh.type = m_br.ReadBytes(m_entryTypeSize);

        //Console.Write("Type: ");
        //for (int i = 0; i < m_entryTypeSize; i++)
        //  Console.Write((Char)eh.type[i]);
        //Console.Write(Environment.NewLine);

        // Block size.
        eh.blockSize = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // failsafe: Read error, return immediately.
        if (eh.blockSize != m_rootHeader.blockSize)
          break;

        entrycount++;

        // Length of the entry in bytes.
        eh.byteCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Length of the entry in blocks.
        eh.blockCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
        //Console.WriteLine("BlockCount: " + eh.blockCount);

        // Burst.
        eh.burst = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Gap.
        eh.gap = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Filename.
        eh.filename = m_br.ReadBytes(m_filenameSize);

        //Console.Write("Filename: ");
        //for (int i = 0; i < m_filenameSize; i++)
        //  Console.Write((Char)eh.filename[i]);
        //Console.Write(Environment.NewLine);

        // Number of avatars.
        eh.avatarsCount = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));

        // Offset to the avatars, from the beginning of the disk, in blocks.
        eh.avatars = new UInt32[eh.avatarsCount + 1];
        //Console.Write("Locations of the avatars: ");
        for (int i = 0; i < eh.avatarsCount + 1; i++)
        {
          eh.avatars[i] = Util.ArrayToUint32(m_br.ReadBytes(sizeof(UInt32)));
          //Console.Write(eh.avatars[i] + " ");
        }
        //Console.Write(Environment.NewLine);

        // Add entry header to the list.
        m_entryHeaderList.Add(eh);

        //--
        if (m_blocks.ContainsKey(currentBlock) == false)
          m_blocks[currentBlock] = new BlockInfo();

        BlockInfo block = m_blocks[currentBlock];
        if (block.entryHeaders == null)
          block.entryHeaders = new List<EntryHeader>();

        block.entryHeaders.Add(eh);

        m_blocks[currentBlock] = block;
        //--

        //if (entryFlags.HasFlag(EntryFlags.SpecialFile) == true)
        //{
        //  Console.Write("Special File: ");
        //  for (int i = 0; i < m_filenameSize; i++)
        //    Console.Write((Char)eh.filename[i]);
        //  Console.Write(Environment.NewLine);
        //}

        // Create directory.
        if (entryFlags.HasFlag(EntryFlags.Directory) == true)
        {
          // Store the current position.
          long currentPosition = m_br.BaseStream.Position;

          // For the filename we need to convert from Byte ASCII to Unicode string and trim the padding ('\0').
          Byte[] bytes = Encoding.Convert(Encoding.ASCII, Encoding.Unicode, eh.filename);
          String unicode = Encoding.Unicode.GetString(bytes).Trim('\0');

          String dirPath = currentPath + m_separator + unicode;

          ReadDirectory(eh.avatars[0], 0, dirPath);

          // Return to previous position.
          m_br.BaseStream.Seek(currentPosition, SeekOrigin.Begin);
        }
        else // Write file.
        {
          // [CRIS] special case: Disc label is the header of the FileSystem referenced as a file. Do not create this file.
          if (Encoding.ASCII.GetString(eh.filename).Contains("Disc label") == false &&
              Encoding.ASCII.GetString(eh.type).Equals("*lbl") == false)
          {
            // Store the current position.
            long currentPosition = m_br.BaseStream.Position;

            // For the filename we need to convert from Byte ASCII to Unicode string and trim the padding ('\0').
            Byte[] bytes = Encoding.Convert(Encoding.ASCII, Encoding.Unicode, eh.filename);
            String unicode = Encoding.Unicode.GetString(bytes).Trim('\0');

            String filePath = currentPath + m_separator + unicode;
            //Console.WriteLine("Writing '" + unicode + "'...");
            ReadFile(eh.avatars[0], filePath, eh.byteCount);

            // Return to previous position.
            m_br.BaseStream.Seek(currentPosition, SeekOrigin.Begin);
          }
        }
      }
    }

    private void ReadFile(UInt32 currentBlock, String filePath, UInt32 fileSize)
    {
      try
      {
        m_br.BaseStream.Seek(currentBlock * m_rootHeader.blockSize, SeekOrigin.Begin);

        FileStream fs = new FileStream(filePath, FileMode.Create);
        BinaryWriter bw = new BinaryWriter(fs);

        Byte[] data = m_br.ReadBytes((int)fileSize);

        //--
        if (m_blocks.ContainsKey(currentBlock) == false)
          m_blocks[currentBlock] = new BlockInfo();

        BlockInfo block = m_blocks[currentBlock];
        block.type = BlockType.Data;
        block.data = data;

        m_blocks[currentBlock] = block;
        //--

        bw.Write(data);

        bw.Close();
        fs.Close();
      }
      catch (Exception ex)
      {
        Console.Error.WriteLine(ex);
      }
    }

    #endregion private methods
  }

  #endregion classes
}
