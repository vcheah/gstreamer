// This file was generated by the Gtk# code generator.
// Any changes made will be lost if regenerated.

namespace Gst.Video {

	using System;
	using System.Collections;
	using System.Collections.Generic;
	using System.Runtime.InteropServices;

#region Autogenerated code
	[StructLayout(LayoutKind.Sequential)]
	public partial struct AncillaryMeta : IEquatable<AncillaryMeta> {

		public Gst.Meta Meta;
		public Gst.Video.AncillaryMetaField Field;
		public bool CNotYChannel;
		public ushort Line;
		public ushort Offset;
		public ushort DID;
		public ushort SDIDBlockNumber;
		public ushort DataCount;
		public ushort Data;
		public ushort Checksum;

		public static Gst.Video.AncillaryMeta Zero = new Gst.Video.AncillaryMeta ();

		public static Gst.Video.AncillaryMeta New(IntPtr raw) {
			if (raw == IntPtr.Zero)
				return Gst.Video.AncillaryMeta.Zero;
			return (Gst.Video.AncillaryMeta) Marshal.PtrToStructure (raw, typeof (Gst.Video.AncillaryMeta));
		}

		[DllImport("gstvideo-1.0-0.dll", CallingConvention = CallingConvention.Cdecl)]
		static extern IntPtr gst_ancillary_meta_get_info();

		public static Gst.MetaInfo Info { 
			get {
				IntPtr raw_ret = gst_ancillary_meta_get_info();
				Gst.MetaInfo ret = Gst.MetaInfo.New (raw_ret);
				return ret;
			}
		}

		public bool Equals (AncillaryMeta other)
		{
			return true && Meta.Equals (other.Meta) && Field.Equals (other.Field) && CNotYChannel.Equals (other.CNotYChannel) && Line.Equals (other.Line) && Offset.Equals (other.Offset) && DID.Equals (other.DID) && SDIDBlockNumber.Equals (other.SDIDBlockNumber) && DataCount.Equals (other.DataCount) && Data.Equals (other.Data) && Checksum.Equals (other.Checksum);
		}

		public override bool Equals (object other)
		{
			return other is AncillaryMeta && Equals ((AncillaryMeta) other);
		}

		public override int GetHashCode ()
		{
			return this.GetType ().FullName.GetHashCode () ^ Meta.GetHashCode () ^ Field.GetHashCode () ^ CNotYChannel.GetHashCode () ^ Line.GetHashCode () ^ Offset.GetHashCode () ^ DID.GetHashCode () ^ SDIDBlockNumber.GetHashCode () ^ DataCount.GetHashCode () ^ Data.GetHashCode () ^ Checksum.GetHashCode ();
		}

		private static GLib.GType GType {
			get { return GLib.GType.Pointer; }
		}
#endregion
	}
}
