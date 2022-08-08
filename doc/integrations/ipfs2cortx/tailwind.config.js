/** @type {import('tailwindcss').Config} */
const plugin = require('tailwindcss/plugin')

module.exports = {
  content: ['./pages/**/*.{js,ts,jsx,tsx}', './components/**/*.{js,ts,jsx,tsx}'],
  darkMode: ['class'],
  theme: {
    extend: {
      scale: {
        500: '5',
        300: '3',
      },
      animation: {
        'spin-bezier': 'myspin 1s cubic-bezier(0.9, 0.26, 0.97, 1) infinite',
      },
      keyframes: {
        wiggle: {
          '0%, 100%': { transform: 'rotate(-3deg)' },
          '50%': { transform: 'rotate(3deg)' },
        },
        myspin: {
          from: { transform: 'rotate(0deg)' },
          to: { transform: 'rotate(360deg)' },
        },
      },
      colors: {
        navy: '#0b3a53',
        'navy-muted': '#244e66',
        aqua: '#69c4cd',
        'aqua-muted': '#9ad4db',
        gray: '#b7bbc8',
        'gray-muted': '#d9dbe2',
        charcoal: '#34373f',
        'charcoal-muted': '#7f8491',
        red: '#ea5037',
        'red-muted': '#f36149',
        yellow: '#f39021',
        'yellow-muted': '#f9a13e',
        teal: '#378085',
        'teal-muted': '#439a9d',
        green: '#0cb892',
        'green-muted': '#0aca9f',
        snow: '#edf0f4',
        'snow-muted': '#f7f8fa',
        link: '#117eb3',
        'washed-blue': '#F0F6FA',
      },
      backgroundImage: {
        'mybg-light': 'linear-gradient(193deg, #edf0f4, 50%, #9ad4db)',
        'mybg-dark': 'linear-gradient(193deg, #244e66, 20%, #0f172a)',
      },
    },
  },
  plugins: [
    require('@tailwindcss/typography'),
    require('@tailwindcss/line-clamp'),
    plugin(function ({ addUtilities }) {
      addUtilities({
        '.scrollbar-hide': {
          /* IE and Edge */
          '-ms-overflow-style': 'none',

          /* Firefox */
          'scrollbar-width': 'none',

          /* Safari and Chrome */
          '&::-webkit-scrollbar': {
            display: 'none',
          },
        },
      })
    }),
    plugin(function ({ addComponents }) {
      addComponents({
        // '.btn': {
        //   padding: '.5rem 1rem !important',
        //   borderRadius: '.25rem !important',
        //   fontWeight: '600 !important',
        // },
        '.mySpinner': {
          display: 'flex',
          // animation: 'spin 1s cubic-bezier(0.83, 0, 0.17, 1) infinite',
          transform: 'translateZ(0)',
          borderTop: '1px solid transparent',
          borderRight: '1px solid transparent',
          borderBottom: '11px solid',
          borderLeft: '2px solid',
          background: 'transparent',
          width: '114px',
          height: '114px',
          borderRadius: '50%',
          transition: '250ms ease border-color',
          alignItems: 'center',
        },
      })
    }),
  ],
}
