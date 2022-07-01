export function DopeAlter({ color, headText, bodyText }) {
  return (
    <div
      className={`w-4/5 sm:w-fit flex flex-col bg-gradient-to-b from-slate-900 border-t border-b border-${
        color || 'slate-900'
      } text-snow px-6 py-3 mb-4 sm:mx-auto`}
      role="alert"
    >
      <p className="font-semibold">{headText}</p>
      <p className="text-sm ">{bodyText}</p>
    </div>
  )
}
